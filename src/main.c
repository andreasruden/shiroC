#include "ast/node.h"
#include "codegen/llvm/llvm_codegen.h"
#include "common/debug/panic.h"
#include "compiler_error.h"
#include "parser/parser.h"
#include "sema/decl_collector.h"
#include "sema/semantic_analyzer.h"
#include "sema/semantic_context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/wait.h>

static char* read_file(const char* filepath)
{
    FILE* file = fopen(filepath, "rb");
    if (!file)
    {
        fprintf(stderr, "Error: Could not open file '%s'\n", filepath);
        return NULL;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    size_t size = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate buffer and read
    char* buffer = malloc(size + 1);
    if (!buffer)
    {
        fprintf(stderr, "Error: Out of memory\n");
        fclose(file);
        return NULL;
    }

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

static FILE* open_output_file_for(const char* sourcefile, char** output_path)
{
    // Extract just the filename (after the last '/')
    const char *filename = strrchr(sourcefile, '/');
    if (filename) {
        filename++; // Skip the '/'
    } else {
        filename = sourcefile; // No slash found, use the whole path
    }

    // Find the last dot in the filename
    const char *last_dot = strrchr(filename, '.');

    // Check if filename ends with ".shiro"
    if (last_dot && strcmp(last_dot, ".shiro") == 0) {
        // Has .shiro extension - replace it with .ll
        size_t base_len = (size_t)last_dot - (size_t)filename;
        *output_path = malloc(base_len + 4);  // +4 for ".ll\0"
        panic_if(!*output_path);
        memcpy(*output_path, filename, base_len);
        strcpy(*output_path + base_len, ".ll");
    } else {
        // No .shiro extension - append .ll
        *output_path = malloc(strlen(filename) + 4);  // +4 for ".ll\0"
        panic_if(!*output_path);
        strcpy(*output_path, filename);
        strcat(*output_path, ".ll");
    }

    FILE *file = fopen(*output_path, "w");
    if (!file) {
        fprintf(stderr, "Unable to open %s for writing", *output_path);
        free(*output_path);
        return nullptr;
    }

    return file;
}

static int compile_with_clang(const char* filepath, const char* output_redirect)
{
    char *output_name;
    if (output_redirect == nullptr)
    {
        // Remove ".ll" extension
        size_t len = strlen(filepath);
        output_name = malloc(len + 1);
        strcpy(output_name, filepath);
        char *ext = strstr(output_name, ".ll");
        if (ext && ext[3] == '\0') {
            *ext = '\0';
        }
    }
    else
        output_name = strdup(output_redirect);

    // Build command
    size_t cmd_len = strlen("clang  -o  -Wno-override-module") + strlen(filepath) + strlen(output_name) + 1;
    char *command = malloc(cmd_len);
    snprintf(command, cmd_len, "clang %s -o %s -Wno-override-module", filepath, output_name);

    // Execute
    int result = system(command);

    if (result == -1) {
        fprintf(stderr, "failed to execute clang\n");
        return 5;
    }

    free(command);
    free(output_name);
    return WEXITSTATUS(result);
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <file.shiro> [-o FILE]\n", argv[0]);
        return 1;
    }

    const char* output_redirect = nullptr;
    const char* filepath = argv[1];

    if (argc == 4 && strcmp(argv[2], "-o") == 0)
        output_redirect = argv[3];

    // Read source file
    char* source = read_file(filepath);
    if (!source)
        return 1;

    // Parse
    parser_t* parser = parser_create();
    parser_set_source(parser, filepath, source);
    ast_root_t* ast = parser_parse(parser);
    bool failed_parse = vec_size(&parser->errors) > 0;
    if (failed_parse)
        print_compiler_errors(&parser->errors);
    parser_destroy(parser);

    if (!ast || failed_parse)
        return 2;

    // Create semantic context
    semantic_context_t* ctx = semantic_context_create();
    panic_if(ctx == nullptr);

    // First pass: Collect declarations
    decl_collector_t* decl_collector = decl_collector_create(ctx);
    bool decl_success = decl_collector_run(decl_collector, AST_NODE(ast));
    if (!decl_success)
    {
        print_ast_errors(&ctx->error_nodes);
        semantic_context_destroy(ctx);
        decl_collector_destroy(decl_collector);
        ast_node_destroy(ast);
        return 3;
    }

    decl_collector_destroy(decl_collector);
    decl_collector = nullptr;

    // Second pass: Semantic analysis
    semantic_analyzer_t* sema = semantic_analyzer_create(ctx);
    bool sema_success = semantic_analyzer_run(sema, AST_NODE(ast));
    if (!sema_success)
    {
        print_ast_errors(&ctx->error_nodes);
        semantic_context_destroy(ctx);
        semantic_analyzer_destroy(sema);
        ast_node_destroy(ast);
        return 4;
    }

    semantic_analyzer_destroy(sema);
    sema = nullptr;

    if (vec_size(&ctx->warning_nodes) > 0)
        print_ast_errors(&ctx->warning_nodes);

    // Code Generation:
    char* ir_path;
    FILE* fout = open_output_file_for(filepath, &ir_path);
    llvm_codegen_t* llvm = llvm_codegen_create();
    llvm_codegen_generate(llvm, AST_NODE(ast), fout);
    fflush(fout);
    fclose(fout);
    llvm_codegen_destroy(llvm);

    // Invoke clang to compile LLVM IR into binary:
    int clang_res = compile_with_clang(ir_path, output_redirect);

    // Cleanup
    if (output_redirect != nullptr)
        remove(ir_path);
    free(ir_path);
    semantic_context_destroy(ctx);
    ast_node_destroy(ast);
    free(source);

    return clang_res;
}
