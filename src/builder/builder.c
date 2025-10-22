#include "builder.h"

#include "builder/module.h"
#include "common/containers/hash_table.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "common/toml_parser.h"
#include "common/util/path.h"
#include "sema/symbol_table.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static constexpr char BUILD_INSTRUCTIONS_FILENAME[] = "shiro.toml";

builder_t* builder_create(const char* root_dir)
{
    builder_t* builder = malloc(sizeof(*builder));
    panic_if(builder == nullptr);

    *builder = (builder_t){
        .root_dir = strdup(root_dir),
        .modules = HASH_TABLE_INIT(module_destroy_void),
    };

    return builder;
}

void builder_destroy(builder_t* builder)
{
    if (builder == nullptr)
        return;

    free(builder->root_dir);
    hash_table_deinit(&builder->modules);
    free(builder);
}

static bool verify_module_path(builder_t* builder, const char* module_name, char* module_path)
{
    // Check if module_path equals the root directory
    if (strcmp(module_path, builder->root_dir) == 0)
    {
        fprintf(stderr, "Error: module '%s' cannot use the root directory as its src\n", module_name);
        return false;
    }

    // Check if module_path conflicts with any existing module
    hash_table_iter_t itr;
    for (hash_table_iter_init(&itr, &builder->modules); hash_table_iter_has_elem(&itr); hash_table_iter_next(&itr))
    {
        module_t* existing = hash_table_iter_current(&itr)->value;

        if (path_is_subpath_of(existing->src_dir, module_path))
        {
            fprintf(stderr, "Error: module '%s' path is nested inside module '%s' path\n",
                module_name, existing->name);
            return false;
        }

        if (path_is_subpath_of(module_path, existing->src_dir))
        {
            fprintf(stderr, "Error: module '%s' path contains module '%s' path\n",
                module_name, existing->name);
            free(module_path);
            return false;
        }
    }

    return true;
}

static bool extract_module(builder_t* builder, hash_table_t* section, bool lib)
{
    if (section == nullptr)
    {
        fprintf(stderr, "Error: Invalid section in `%s` array", lib ? "lib" : "bin");
        return false;
    }

    const char* module_name = hash_table_find(section, "name");
    const char* module_src = hash_table_find(section, "src");

    if (module_name == nullptr)
    {
        fprintf(stderr, "Error: missing mandatory section `name` in `%s` array", lib ? "lib" : "bin");
        return false;
    }

    if (module_src == nullptr)
    {
        fprintf(stderr, "Error: missing mandatory section `src` in `%s` array", lib ? "lib" : "bin");
        return false;
    }

    char* module_path = join_path(builder->root_dir, module_src);
    if (!verify_module_path(builder, module_name, module_path))
    {
        free(module_path);
        return false;
    }

    module_t* module = module_create(builder, module_name, module_path, lib ? MODULE_LIBRARY : MODULE_BINARY);
    hash_table_insert(&builder->modules, module_name, module);
    free(module_path);

    return true;
}

static bool extract_build_instructions(builder_t* builder)
{
    char* toml_path = nullptr;
    hash_table_t* toml_file = nullptr;
    bool error = false;

    // Parse build instructions
    toml_path = join_path(builder->root_dir, BUILD_INSTRUCTIONS_FILENAME);
    toml_file = toml_parse_file(toml_path);
    if (toml_file == nullptr)
    {
        fprintf(stderr, "Missing or invalid build file %s\n", toml_path);
        error = true;
        goto cleanup;
    }

    // Extract data from project section
    hash_table_t* project_section = toml_as_section(hash_table_find(toml_file, "project"));
    if (project_section == nullptr)
    {
        fprintf(stderr, "Error: Missing mandatory section `project`");
        error = true;
        goto cleanup;
    }
    const char* name = hash_table_find(project_section, "name");
    printf("Building project %s\n", name ? name : "UNNAMED");

    // Construct modules from "bin" array
    vec_t* bins = toml_as_array_section(hash_table_find(toml_file, "bin"));
    if (bins != nullptr)
    {
        for (size_t i = 0; i < vec_size(bins); ++i)
        {
            if (!extract_module(builder, toml_as_section(vec_get(bins, i)), false))
            {
                error = true;
                goto cleanup;
            }
        }
    }

    // Construct modules from "lib" array
    vec_t* libs = toml_as_array_section(hash_table_find(toml_file, "lib"));
    if (libs != nullptr)
    {
        for (size_t i = 0; i < vec_size(libs); ++i)
        {
            hash_table_t* section = toml_as_section(vec_get(libs, i));
            if (!section)
            {
                fprintf(stderr, "Error: Invalid section in `lib` array");
                error = true;
                goto cleanup;
            }

            if (!extract_module(builder, section, true))
            {
                error = true;
                goto cleanup;
            }
        }
    }

cleanup:
    hash_table_destroy(toml_file);
    free(toml_path);
    return !error;
}

static bool for_each_module(builder_t* builder, bool (*fn)(module_t*))
{
    hash_table_iter_t itr;
    for (hash_table_iter_init(&itr, &builder->modules); hash_table_iter_has_elem(&itr); hash_table_iter_next(&itr))
    {
        module_t* module = hash_table_iter_current(&itr)->value;
        if (!fn(module))
            return false;
    }
    return true;
}

static bool inject_exports_into_module(module_t* module)
{
    for (size_t i = 0; i < vec_size(&module->dependencies); ++i)
    {
        module_t* dep_mod = hash_table_find(&module->builder->modules, vec_get(&module->dependencies, i));
        panic_if(dep_mod == nullptr);
        symbol_table_merge(module->sema_context->global, dep_mod->sema_context->export);
    }
    return true;
}

bool builder_run(builder_t* builder)
{
    if (!extract_build_instructions(builder))
        return false;

    // Build AST for every module
    if (!for_each_module(builder, module_parse_src))
        return false;

    // Build symbols for every module with decl collector
    if (!for_each_module(builder, module_decl_collect))
        return false;

    // Using AST, map what dependencies a module has
    if (!for_each_module(builder, module_populate_dependencies))
        return false;

    // TODO: Dependency graph needs to reorder builder->modules or emit error

    // Inject exported symbols from all dependencies into module's global symbols
    // TODO: Collision should be accepted until ambiguity ensues, at which
    // point compiler should emit error and force more explicit naming
    if (!for_each_module(builder, inject_exports_into_module))
        return false;

    // Build modules in dependency defined order
    if (!for_each_module(builder, module_compile))
        return false;

    // Link executable module with its dependencies
    hash_table_iter_t itr;
    for (hash_table_iter_init(&itr, &builder->modules); hash_table_iter_has_elem(&itr); hash_table_iter_next(&itr))
    {
        module_t* module = hash_table_iter_current(&itr)->value;
        if (module->kind == MODULE_BINARY && !module_link(module))
            return false;
    }

    return true;
}
