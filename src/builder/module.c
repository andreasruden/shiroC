#include "module.h"

#include "ast/node.h"
#include "ast/root.h"
#include "builder/builder.h"
#include "codegen/llvm/llvm_codegen.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "common/util/path.h"
#include "compiler_error.h"
#include "parser/parser.h"
#include "sema/decl_collector.h"
#include "sema/semantic_analyzer.h"
#include "sema/semantic_context.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char* read_file(const char* filepath)
{
    FILE* file = fopen(filepath, "rb");
    if (!file)
    {
        fprintf(stderr, "Error: Could not open file '%s'\n", filepath);
        return nullptr;
    }

    fseek(file, 0, SEEK_END);
    size_t size = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = malloc(size + 1);
    panic_if(buffer == nullptr);

    size_t bytes_read = fread(buffer, 1, size, file);
    buffer[bytes_read] = '\0';

    fclose(file);
    return buffer;
}

static void print_compiler_errors(vec_t* vec)
{
    for (size_t j = 0; j < vec_size(vec); j++)
    {
        compiler_error_t* error = vec_get(vec, j);
        char* str = compiler_error_string(error);
        fprintf(stderr, "%s", str);
        free(str);
    }
}

static void print_ast_errors(vec_t* vec)
{
    for (size_t i = 0; i < vec_size(vec); i++)
    {
        ast_node_t* node = vec_get(vec, i);
        print_compiler_errors(node->errors);
    }
}

static bool ends_with(const char* str, const char* suffix)
{
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len)
        return false;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

module_t* module_create(builder_t* builder, const char* name, const char* src_dir, module_kind_t kind)
{
    module_t* module = malloc(sizeof(*module));
    panic_if(module == nullptr);

    *module = (module_t){
        .builder = builder,
        .kind = kind,
        .name = strdup(name),
        .src_dir = strdup(src_dir),
        .ast = VEC_INIT(ast_node_destroy),
        .dependencies = VEC_INIT(free),
        .sema_context = semantic_context_create(),
    };

    return module;
}

void module_destroy(module_t* module)
{
    if (module == nullptr)
        return;

    free(module->name);
    free(module->src_dir);
    vec_deinit(&module->ast);
    vec_deinit(&module->dependencies);
    semantic_context_destroy(module->sema_context);
    free(module);
}

void module_destroy_void(void* module)
{
    module_destroy(module);
}

static bool parse_directory_recursive(module_t* module, parser_t* parser, const char* dir_path)
{
    DIR* dir = opendir(dir_path);
    if (!dir)
    {
        fprintf(stderr, "Error: Could not open directory '%s'\n", dir_path);
        return false;
    }

    bool success = true;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        // Skip current and parent directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char* entry_path = join_path(dir_path, entry->d_name);

        // Check if entry is a directory (handle DT_UNKNOWN by checking with stat)
        bool is_directory = false;
        if (entry->d_type == DT_DIR)
        {
            is_directory = true;
        }
        else if (entry->d_type == DT_UNKNOWN)
        {
            struct stat statbuf;
            if (stat(entry_path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode))
                is_directory = true;
        }

        if (is_directory)
        {
            // Recursively parse subdirectory
            if (!parse_directory_recursive(module, parser, entry_path))
                success = false;
            free(entry_path);
            continue;
        }

        // Skip non-regular files that aren't .shiro files
        if (entry->d_type != DT_REG && entry->d_type != DT_UNKNOWN)
        {
            free(entry_path);
            continue;
        }

        if (!ends_with(entry->d_name, ".shiro"))
        {
            free(entry_path);
            continue;
        }

        // Parse .shiro file
        char* source = read_file(entry_path);
        if (!source)
        {
            fprintf(stderr, "Error: Failed to read file '%s'\n", entry_path);
            free(entry_path);
            success = false;
            continue;
        }

        printf("  %s\n", entry_path);
        parser_set_source(parser, entry_path, source);
        ast_root_t* ast = parser_parse(parser);
        bool failed_parse = vec_size(&parser->errors) > 0;

        if (failed_parse)
            print_compiler_errors(&parser->errors);

        free(source);
        free(entry_path);

        if (!ast || failed_parse)
        {
            ast_node_destroy(ast);
            success = false;
            continue;
        }

        // Add AST to module
        vec_push(&module->ast, ast);
    }

    closedir(dir);
    return success;
}

bool module_parse_src(module_t* module)
{
    printf("Parsing module %s\n", module->name);

    parser_t* parser = parser_create();
    bool success = parse_directory_recursive(module, parser, module->src_dir);
    parser_destroy(parser);

    return success;
}

bool module_decl_collect(module_t* module)
{
    printf("Building symbols of module %s\n", module->name);

    semantic_context_register_builtins(module->sema_context);

    decl_collector_t* decl_collector = decl_collector_create(module->sema_context);

    // Run declaration collection on all AST nodes
    bool success = true;
    for (size_t i = 0; i < vec_size(&module->ast); ++i)
    {
        ast_node_t* ast = vec_get(&module->ast, i);
        if (!decl_collector_run(decl_collector, ast))
            success = false;
    }

    if (!success)
        print_ast_errors(&module->sema_context->error_nodes);

    decl_collector_destroy(decl_collector);

    return success;
}

bool module_populate_dependencies(module_t* module)
{
    // TODO: Add deps

    printf("Module %s depends on:\n", module->name);
    for (size_t i = 0; i < vec_size(&module->dependencies); ++i)
        printf("  - %s\n", (const char*)vec_get(&module->dependencies, i));
    return true;
}

bool module_compile(module_t* module)
{
    printf("Compiling module %s\n", module->name);

    semantic_analyzer_t* sema = semantic_analyzer_create(module->sema_context);

    // Run semantic analysis on all AST nodes
    bool success = true;
    for (size_t i = 0; i < vec_size(&module->ast); ++i)
    {
        ast_node_t* ast = vec_get(&module->ast, i);
        if (!semantic_analyzer_run(sema, ast))
            success = false;
    }

    semantic_analyzer_destroy(sema);

    if (!success)
    {
        print_ast_errors(&module->sema_context->error_nodes);
        return false;
    }

    // Warnings
    if (vec_size(&module->sema_context->warning_nodes) > 0)
        print_ast_errors(&module->sema_context->warning_nodes);

    // FIXME: Generate LLVM IR for each AST (currently we have to output to .ll files; fix that first)

    return true;
}

bool module_link(module_t* module)
{
    panic_if(module->kind != MODULE_BINARY);

    printf("Linking module %s\n", module->name);
    return true;
}
