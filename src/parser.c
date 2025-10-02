#include "parser.h"

#include "ast/def/def.h"
#include "ast/def/fn_def.h"
#include "ast/expr/expr.h"
#include "ast/expr/int_lit.h"
#include "ast/node.h"
#include "ast/root.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "lexer.h"

#include <stdlib.h>
#include <string.h>

// TODO: Errors

ast_stmt_t* parse_stmt(parser_t* parser);

parser_t* parser_create(lexer_t* lexer)
{
    parser_t* parser = malloc(sizeof(*parser));

    parser->lexer = lexer;

    return parser;
}

void parser_destroy(parser_t* parser)
{
    if (parser != nullptr)
        free(parser);
}

ast_int_lit_t* parse_int_lit(parser_t* parser)
{
    token_t* tok = lexer_next_token_iff(parser->lexer, TOKEN_NUMBER);
    if (tok == nullptr)
        return nullptr;

    int value = atoi(tok->value);
    token_destroy(tok);
    return ast_int_lit_create(value);
}

ast_expr_t* parse_expr(parser_t* parser)
{
    if (lexer_peek_token(parser->lexer)->type == TOKEN_NUMBER)
        return (ast_expr_t*)parse_int_lit(parser);
    return nullptr;
}

ast_return_stmt_t* parse_return_stmt(parser_t* parser)
{
    if (!lexer_consume_token(parser->lexer, TOKEN_RETURN))
        return nullptr;

    ast_expr_t* expr = parse_expr(parser);
    if (expr == nullptr)
        return nullptr;

    if (!lexer_consume_token(parser->lexer, TOKEN_SEMICOLON))
    {
        ast_node_destroy(AST_NODE(expr));
        return nullptr;
    }

    return ast_return_stmt_create(expr);
}

ast_compound_stmt_t* parse_compound_stmt(parser_t* parser)
{
    if (!lexer_consume_token(parser->lexer, TOKEN_LBRACE))
        return nullptr;

    ast_stmt_t* inner_stmt = parse_stmt(parser);
    if (inner_stmt == nullptr)
        return nullptr;

    if (!lexer_consume_token(parser->lexer, TOKEN_RBRACE))
        return nullptr;

    return ast_compound_stmt_create(inner_stmt);
}

ast_stmt_t* parse_stmt(parser_t* parser)
{
    if (lexer_peek_token(parser->lexer)->type == TOKEN_RETURN)
        return (ast_stmt_t*)parse_return_stmt(parser);
    return nullptr;
}

ast_fn_def_t* parse_fn_def(parser_t* parser)
{
    if (!lexer_consume_token(parser->lexer, TOKEN_INT))
        return nullptr;

    token_t* id = lexer_next_token_iff(parser->lexer, TOKEN_IDENTIFIER);
    if (id == nullptr || strcmp(id->value, "main") != 0)
        return nullptr;

    if (!lexer_consume_token(parser->lexer, TOKEN_LPAREN))
        return nullptr;

    if (!lexer_consume_token(parser->lexer, TOKEN_RPAREN))
        return nullptr;

    ast_compound_stmt_t* body = parse_compound_stmt(parser);
    if (body == nullptr)
        return nullptr;

    return ast_fn_def_create(id->value, body);
}

ast_def_t* parse_top_level_definition(parser_t* parser)
{
    return (ast_def_t*)parse_fn_def(parser);
}

ast_root_t* parser_parse(parser_t* parser)
{
    ast_def_t* node = parse_top_level_definition(parser);
    if (node == nullptr)
        return nullptr;

    return ast_root_create(node);
}
