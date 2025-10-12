#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "ast/node.h"
#include "parser/parser.h"
#include "sema/decl_collector.h"
#include "sema/semantic_analyzer.h"
#include "sema/semantic_context.h"

// This is the entry point libFuzzer calls
int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Ignore empty inputs
    if (size == 0)
        return 0;

    // Ensure null-terminated string
    char* source = (char*)malloc(size + 1);
    if (!source)
        return 0;
    memcpy(source, data, size);
    source[size] = '\0';

    // Lexing/Parsing
    parser_t* parser = parser_create();
    parser_set_source(parser, "fuzzer", source);
    ast_root_t* ast = parser_parse(parser);

    if (ast != nullptr && vec_size(&parser->errors) == 0)
    {
        semantic_context_t* ctx = semantic_context_create();
        decl_collector_t* decl_collector = decl_collector_create(ctx);
        bool res = decl_collector_run(decl_collector, AST_NODE(ast));

        if (res)
        {
            semantic_analyzer_t* analyzer = semantic_analyzer_create(ctx);
            semantic_analyzer_run(analyzer, AST_NODE(ast));
            semantic_analyzer_destroy(analyzer);
        }

        decl_collector_destroy(decl_collector);
        semantic_context_destroy(ctx);
    }

    ast_node_destroy(ast);
    parser_destroy(parser);

    free(source);

    // Always return 0 (success) - libFuzzer handles crashes/sanitizer errors
    return 0;
}
