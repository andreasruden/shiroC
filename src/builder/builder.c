#include "builder.h"

#include "builder/module.h"
#include "common/containers/hash_table.h"
#include "common/containers/string.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "common/toml_parser.h"
#include "common/util/path.h"
#include "common/util/ssprintf.h"
#include "sema/semantic_context.h"
#include "sema/symbol_table.h"

#include <ctype.h>
#include <libgen.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static constexpr char BUILD_INSTRUCTIONS_FILENAME[] = "shiro.toml";

dependency_t* dependency_create(const char* name, const char* src_path)
{
    dependency_t* dep = malloc(sizeof(*dep));
    panic_if(dep == nullptr);

    *dep = (dependency_t){
        .name = strdup(name),
        .src_path = strdup(src_path),
        .modules = VEC_INIT(nullptr),  // Don't own modules - they're owned by builder->modules
        .project_name = nullptr,  // Set when parsing dependency's shiro.toml
    };

    return dep;
}

void dependency_destroy(dependency_t* dep)
{
    if (dep == nullptr)
        return;

    free(dep->name);
    free(dep->src_path);
    free(dep->project_name);
    vec_deinit(&dep->modules);
    free(dep);
}

void dependency_destroy_void(void* dep)
{
    dependency_destroy(dep);
}

builder_t* builder_create(const char* root_dir, const char* compiler_path)
{
    builder_t* builder = malloc(sizeof(*builder));
    panic_if(builder == nullptr);

    *builder = (builder_t){
        .root_dir = strdup(root_dir),
        .project = nullptr,    // Set when parsing BUILD_INSTRUCTIONS_FILENAME
        .build_dir = nullptr,  // Set later once we know the project name
        .bin_dir = nullptr,    // Set later once we know the project name
        .modules = HASH_TABLE_INIT(module_destroy_void),
        .dependencies = VEC_INIT(dependency_destroy_void),
    };

    // TODO: This resolution is just for developing
    // Resolve std library path relative to compiler binary
    // Compiler is at build/bin/shiro, std is at src/std
    char* compiler_path_copy = strdup(compiler_path);
    char* compiler_dir = dirname(compiler_path_copy);

    // Build path: <compiler_dir>/../../src/std
    string_t std_path_rel = STRING_INIT;
    string_append_cstr(&std_path_rel, ssprintf("%s/../../src/std", compiler_dir));
    free(compiler_path_copy);

    // Canonicalize the path
    char* std_path = realpath(string_cstr(&std_path_rel), nullptr);
    string_deinit(&std_path_rel);

    if (std_path != nullptr)
    {
        // Verify std directory exists
        struct stat path_stat;
        if (stat(std_path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode))
        {
            // Add std as first implicit dependency
            dependency_t* std_dep = dependency_create("std", std_path);
            vec_push(&builder->dependencies, std_dep);
        }
        else
        {
            fprintf(stderr, "Warning: std library not found at %s, continuing without it\n", std_path);
        }
        free(std_path);
    }
    else
    {
        fprintf(stderr, "Warning: Could not resolve std library path, continuing without it\n");
    }

    return builder;
}

void builder_destroy(builder_t* builder)
{
    if (builder == nullptr)
        return;

    free(builder->project);
    free(builder->root_dir);
    free(builder->build_dir);
    free(builder->bin_dir);
    hash_table_deinit(&builder->modules);
    vec_deinit(&builder->dependencies);
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

static bool verify_name(const char* name)
{
    const char* ptr = name;

    // Must start with letter
    if (!name || *ptr == '\0' || !isalpha(*ptr))
    {
        fprintf(stderr, "Error: name '%s' does not start with an ASCII letter\n", name);
        return false;
    }

    // Must only contain letters, digits, '_' and '-'
    while (*ptr != '\0')
    {
        if (!isalnum(*ptr) && *ptr != '_' && *ptr != '-')
        {
            fprintf(stderr, "Error: name '%s' contains unallowed characters\n", name);
            return false;
        }

        ++ptr;
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

    if (!verify_name(module_name))
        return false;

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

static bool extract_dependency(builder_t* builder, hash_table_t* section)
{
    if (section == nullptr)
    {
        fprintf(stderr, "Error: Invalid section in `dep` array\n");
        return false;
    }

    const char* dep_name = hash_table_find(section, "name");
    const char* dep_src = hash_table_find(section, "src");

    // Validate name field
    if (dep_name == nullptr)
    {
        fprintf(stderr, "Error: missing mandatory field `name` in `dep` array\n");
        return false;
    }

    if (!verify_name(dep_name))
        return false;

    // Validate src field
    if (dep_src == nullptr)
    {
        fprintf(stderr, "Error: missing mandatory field `src` in `dep` array\n");
        return false;
    }

    // Resolve dependency path relative to project root
    char* resolved_dep_path = join_path(builder->root_dir, dep_src);

    // Check if src path exists
    struct stat path_stat;
    if (stat(resolved_dep_path, &path_stat) != 0)
    {
        fprintf(stderr, "Error: dependency src path '%s' does not exist\n", resolved_dep_path);
        free(resolved_dep_path);
        return false;
    }

    if (!S_ISDIR(path_stat.st_mode))
    {
        fprintf(stderr, "Error: dependency src path '%s' is not a directory\n", resolved_dep_path);
        free(resolved_dep_path);
        return false;
    }

    // Check for duplicate dependency names
    for (size_t i = 0; i < vec_size(&builder->dependencies); ++i)
    {
        dependency_t* existing = vec_get(&builder->dependencies, i);
        if (strcmp(existing->name, dep_name) == 0)
        {
            fprintf(stderr, "Error: duplicate dependency name '%s'\n", dep_name);
            free(resolved_dep_path);
            return false;
        }
    }

    // Create and add dependency with resolved path
    dependency_t* dep = dependency_create(dep_name, resolved_dep_path);
    vec_push(&builder->dependencies, dep);
    free(resolved_dep_path);

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
    if (name == nullptr)
    {
        fprintf(stderr, "Error: Missing mandatory field `name` in `project` section");
        error = true;
        goto cleanup;
    }
    if (!verify_name(name))
    {
        error = true;
        goto cleanup;
    }
    builder->project = strdup(name);
    printf("Building project %s\n", name);

    // Set build directory: ./build/<project_name>/
    builder->build_dir = join_path("build", name);
    builder->bin_dir = join_path(builder->build_dir, "bin");

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

    // Parse dependencies from "dep" array
    vec_t* deps = toml_as_array_section(hash_table_find(toml_file, "dep"));
    if (deps != nullptr)
    {
        for (size_t i = 0; i < vec_size(deps); ++i)
        {
            hash_table_t* section = toml_as_section(vec_get(deps, i));
            if (!section)
            {
                fprintf(stderr, "Error: Invalid section in `dep` array\n");
                error = true;
                goto cleanup;
            }

            if (!extract_dependency(builder, section))
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

static bool builder_load_dependency(builder_t* builder, dependency_t* dep)
{
    char* toml_path = nullptr;
    hash_table_t* toml_file = nullptr;
    bool error = false;

    // Parse dependency's shiro.toml
    toml_path = join_path(dep->src_path, BUILD_INSTRUCTIONS_FILENAME);
    toml_file = toml_parse_file(toml_path);
    if (toml_file == nullptr)
    {
        fprintf(stderr, "Error: Missing or invalid build file for dependency '%s' at %s\n",
            dep->name, toml_path);
        error = true;
        goto cleanup;
    }

    // Extract project name from dependency's [project] section
    hash_table_t* project_section = toml_as_section(hash_table_find(toml_file, "project"));
    if (project_section == nullptr)
    {
        fprintf(stderr, "Error: Missing mandatory section `project` in dependency '%s'\n", dep->name);
        error = true;
        goto cleanup;
    }

    const char* project_name = hash_table_find(project_section, "name");
    if (project_name == nullptr)
    {
        fprintf(stderr, "Error: Missing mandatory field `name` in `project` section of dependency '%s'\n",
            dep->name);
        error = true;
        goto cleanup;
    }

    if (!verify_name(project_name))
    {
        error = true;
        goto cleanup;
    }

    dep->project_name = strdup(project_name);
    printf("Loading dependency %s (project: %s)\n", dep->name, project_name);

    // Process [[bin]] modules from dependency
    vec_t* bins = toml_as_array_section(hash_table_find(toml_file, "bin"));
    if (bins != nullptr)
    {
        for (size_t i = 0; i < vec_size(bins); ++i)
        {
            hash_table_t* section = toml_as_section(vec_get(bins, i));
            if (!section)
            {
                fprintf(stderr, "Error: Invalid section in `bin` array of dependency '%s'\n", dep->name);
                error = true;
                goto cleanup;
            }

            const char* module_name = hash_table_find(section, "name");
            const char* module_src = hash_table_find(section, "src");

            if (module_name == nullptr || !verify_name(module_name))
            {
                error = true;
                goto cleanup;
            }

            if (module_src == nullptr)
            {
                fprintf(stderr, "Error: missing mandatory section `src` in `bin` array of dependency '%s'\n",
                    dep->name);
                error = true;
                goto cleanup;
            }

            char* module_path = join_path(dep->src_path, module_src);
            module_t* module = module_create(builder, module_name, module_path, MODULE_BINARY);
            module->is_dependency = true;
            module->project_name = strdup(dep->project_name);

            // Add to builder's module map with namespaced key: "dep_name.module_name"
            const char* namespaced_key = ssprintf("%s.%s", dep->name, module_name);
            hash_table_insert(&builder->modules, namespaced_key, module);

            // Add to dependency's module vector
            vec_push(&dep->modules, module);
            free(module_path);
        }
    }

    // Process [[lib]] modules from dependency
    vec_t* libs = toml_as_array_section(hash_table_find(toml_file, "lib"));
    if (libs != nullptr)
    {
        for (size_t i = 0; i < vec_size(libs); ++i)
        {
            hash_table_t* section = toml_as_section(vec_get(libs, i));
            if (!section)
            {
                fprintf(stderr, "Error: Invalid section in `lib` array of dependency '%s'\n", dep->name);
                error = true;
                goto cleanup;
            }

            const char* module_name = hash_table_find(section, "name");
            const char* module_src = hash_table_find(section, "src");

            if (module_name == nullptr || !verify_name(module_name))
            {
                error = true;
                goto cleanup;
            }

            if (module_src == nullptr)
            {
                fprintf(stderr, "Error: missing mandatory section `src` in `lib` array of dependency '%s'\n",
                    dep->name);
                error = true;
                goto cleanup;
            }

            char* module_path = join_path(dep->src_path, module_src);
            module_t* module = module_create(builder, module_name, module_path, MODULE_LIBRARY);
            module->is_dependency = true;
            module->project_name = strdup(dep->project_name);

            // Add to builder's module map with namespaced key: "dep_name.module_name"
            const char* namespaced_key = ssprintf("%s.%s", dep->name, module_name);
            hash_table_insert(&builder->modules, namespaced_key, module);

            // Add to dependency's module vector
            vec_push(&dep->modules, module);
            free(module_path);
        }
    }

cleanup:
    hash_table_destroy(toml_file);
    free(toml_path);
    return !error;
}

static bool builder_load_all_dependencies(builder_t* builder)
{
    for (size_t i = 0; i < vec_size(&builder->dependencies); ++i)
    {
        dependency_t* dep = vec_get(&builder->dependencies, i);
        if (!builder_load_dependency(builder, dep))
            return false;
    }
    return true;
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
        const char* dep_name = vec_get(&module->dependencies, i);
        module_t* dep_mod = hash_table_find(&module->builder->modules, dep_name);
        panic_if(dep_mod == nullptr);

        // Register namespace symbols for qualified name resolution
        const char* project_ns_name = dep_mod->is_dependency ? dep_mod->project_name : "Self";
        symbol_t* project_ns = symbol_table_lookup_local(module->sema_context->global, project_ns_name);
        if (project_ns == nullptr)
        {
            project_ns = semantic_context_register_namespace(module->sema_context, nullptr, project_ns_name,
                nullptr);
        }
        symbol_t* mod_ns = semantic_context_register_namespace(module->sema_context, project_ns, dep_mod->name,
            nullptr);  // don't copy our exports, symbol_table_import will make appropriate clones

        symbol_table_import(module->sema_context->global, dep_mod->sema_context->exports, mod_ns);
    }
    return true;
}

bool builder_run(builder_t* builder)
{
    if (!extract_build_instructions(builder))
        return false;

    // Load all dependency projects
    if (!builder_load_all_dependencies(builder))
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

    // TODO: Verify dependencies are not circular

    // Inject exported symbols from all dependencies into module's global symbols
    if (!for_each_module(builder, inject_exports_into_module))
        return false;

    // Compile dependency modules first
    for (size_t i = 0; i < vec_size(&builder->dependencies); ++i)
    {
        dependency_t* dep = vec_get(&builder->dependencies, i);
        for (size_t j = 0; j < vec_size(&dep->modules); ++j)
        {
            module_t* module = vec_get(&dep->modules, j);
            if (!module_compile(module))
                return false;
        }
    }

    // Then compile main project modules
    hash_table_iter_t compile_itr;
    for (hash_table_iter_init(&compile_itr, &builder->modules); hash_table_iter_has_elem(&compile_itr);
        hash_table_iter_next(&compile_itr))
    {
        module_t* module = hash_table_iter_current(&compile_itr)->value;
        if (!module->is_dependency && !module_compile(module))
            return false;
    }

    // Link executable module with its dependencies
    hash_table_iter_t link_itr;
    for (hash_table_iter_init(&link_itr, &builder->modules); hash_table_iter_has_elem(&link_itr);
        hash_table_iter_next(&link_itr))
    {
        module_t* module = hash_table_iter_current(&link_itr)->value;
        if (module->kind == MODULE_BINARY && !module_link(module))
            return false;
    }

    return true;
}
