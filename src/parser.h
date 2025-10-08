#ifndef PARSER__H
#define PARSER__H

#include "ast/decl/decl.h"
#include "ast/expr/expr.h"
#include "ast/stmt/stmt.h"
#include "ast/root.h"
#include "lexer.h"

typedef struct parser
{
    lexer_t* lexer;
    vec_t lex_errors;   // only lex errors: we own those
    vec_t errors;       // lex + AST errors
} parser_t;

parser_t* parser_create();

void parser_destroy(parser_t* parser);

// This calls parser_reset().
void parser_set_source(parser_t* parser, const char* filename, const char* source);

// Reset current parse position to start of source code & forget all errors encountered.
void parser_reset(parser_t* parser);

// Returns errors (vec<compiler_error*>) encountered by the parser.
// This vector can change/become invalid on future calls to the parser.
vec_t* parser_errors(parser_t* parser);

// This calls parser_reset() before parsing.
ast_root_t* parser_parse(parser_t* parser);

ast_expr_t* parser_parse_primary_expr(parser_t* parser);

ast_expr_t* parser_parse_expr(parser_t* parser);

ast_stmt_t* parser_parse_stmt(parser_t* parser);

#endif
