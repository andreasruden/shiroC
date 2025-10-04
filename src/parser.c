#include "parser.h"

#include "ast/decl/param_decl.h"
#include "ast/def/def.h"
#include "ast/def/fn_def.h"
#include "ast/expr/call_expr.h"
#include "ast/expr/expr.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/ref_expr.h"
#include "ast/node.h"
#include "ast/root.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/expr_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "common/containers/ptr_vec.h"
#include "lexer.h"

#include <stdlib.h>

// TODO: Errors

ast_expr_t* parse_expr(parser_t* parser);
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
    {
        lexer_destroy(parser->lexer);
        free(parser);
    }
}

ast_expr_t* parse_int_lit(parser_t* parser)
{
    token_t* tok = lexer_next_token_iff(parser->lexer, TOKEN_NUMBER);
    if (tok == nullptr)
        return nullptr;

    int value = atoi(tok->value);
    token_destroy(tok);
    return ast_int_lit_create(value);
}

ast_expr_t* parse_call_expr(parser_t* parser, const char* name)
{
    ast_expr_t* call = nullptr;
    ptr_vec_t args = PTR_VEC_INIT;

    if (!lexer_consume_token(parser->lexer, TOKEN_LPAREN))
        goto cleanup;

    token_type_t type;
    while ((type = lexer_peek_token(parser->lexer)->type) != TOKEN_EOF && type != TOKEN_RPAREN)
    {
        ast_expr_t* arg = parse_expr(parser);
        if (arg == nullptr)
            goto cleanup;
        ptr_vec_append(&args, arg);

        if (lexer_peek_token(parser->lexer)->type != TOKEN_COMMA)
            break;
        lexer_consume_token(parser->lexer, TOKEN_COMMA);
    }

    if (!lexer_consume_token(parser->lexer, TOKEN_RPAREN))
        goto cleanup;

    call = ast_call_expr_create(ast_ref_expr_create(name), &args);

cleanup:
    ptr_vec_deinit(&args);
    return call;
}

ast_expr_t* parse_identifier_expr(parser_t* parser)
{
    ast_expr_t* expr = nullptr;

    token_t* id = lexer_next_token_iff(parser->lexer, TOKEN_IDENTIFIER);
    if (id == nullptr)
        goto cleanup;

    if (lexer_peek_token(parser->lexer)->type == TOKEN_LPAREN)
        expr = parse_call_expr(parser, id->value);
    else
        expr = ast_ref_expr_create(id->value);

cleanup:
    token_destroy(id);
    return expr;
}

ast_expr_t* parse_expr(parser_t* parser)
{
    switch (lexer_peek_token(parser->lexer)->type)
    {
        case TOKEN_NUMBER:
            return parse_int_lit(parser);
        case TOKEN_IDENTIFIER:
            return parse_identifier_expr(parser);
        default:
            break;
    }
    return nullptr;
}

ast_stmt_t* parse_return_stmt(parser_t* parser)
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

ast_stmt_t* parse_compound_stmt(parser_t* parser)
{
    if (!lexer_consume_token(parser->lexer, TOKEN_LBRACE))
        return nullptr;

    ptr_vec_t inner_stmts = PTR_VEC_INIT;
    token_type_t type;
    while ((type = lexer_peek_token(parser->lexer)->type) != TOKEN_EOF && type != TOKEN_RBRACE)
    {
        ast_stmt_t* inner_stmt = parse_stmt(parser);
        if (inner_stmt == nullptr)
        {
            ptr_vec_deinit(&inner_stmts);
            return nullptr;
        }
        ptr_vec_append(&inner_stmts, inner_stmt);
    }

    if (!lexer_consume_token(parser->lexer, TOKEN_RBRACE))
    {
        ptr_vec_deinit(&inner_stmts);
        return nullptr;
    }

    return ast_compound_stmt_create(&inner_stmts);
}

ast_stmt_t* parse_expr_stmt(parser_t* parser)
{
    ast_expr_t* expr = parse_expr(parser);
    if (expr == nullptr)
        return nullptr;
    if (!lexer_consume_token(parser->lexer, TOKEN_SEMICOLON))
    {
        ast_node_destroy(AST_NODE(expr));
        return nullptr;
    }
    return ast_expr_stmt_create(expr);
}

ast_stmt_t* parse_stmt(parser_t* parser)
{
    switch (lexer_peek_token(parser->lexer)->type)
    {
        case TOKEN_RETURN:
            return parse_return_stmt(parser);
        case TOKEN_LBRACE:
            return parse_compound_stmt(parser);
        default:
            return parse_expr_stmt(parser);
    }
}

ast_param_decl_t* parse_param_decl(parser_t* parser)
{
    token_t* type_tok = lexer_next_token(parser->lexer);
    token_t* name_tok = lexer_next_token(parser->lexer);

    ast_param_decl_t* decl = nullptr;
    if (type_tok->type != TOKEN_INT && type_tok->type != TOKEN_IDENTIFIER)
        goto cleanup;
    if (name_tok->type != TOKEN_IDENTIFIER)
        goto cleanup;
    decl = ast_param_decl_create(type_tok->value, name_tok->value);

cleanup:
    token_destroy(type_tok);
    token_destroy(name_tok);
    return decl;
}

ast_def_t* parse_fn_def(parser_t* parser)
{
    token_t* id = nullptr;
    ptr_vec_t params = PTR_VEC_INIT;
    ast_def_t* fn_def = nullptr;

    if (!lexer_consume_token(parser->lexer, TOKEN_INT))
        goto cleanup;

    id = lexer_next_token_iff(parser->lexer, TOKEN_IDENTIFIER);
    if (id == nullptr)
        goto cleanup;

    if (!lexer_consume_token(parser->lexer, TOKEN_LPAREN))
        goto cleanup;

    token_type_t type;
    while ((type = lexer_peek_token(parser->lexer)->type) != TOKEN_EOF && type != TOKEN_RPAREN)
    {
        ast_param_decl_t* param = parse_param_decl(parser);
        if (param == nullptr)
            goto cleanup;
        ptr_vec_append(&params, param);

        if (lexer_peek_token(parser->lexer)->type != TOKEN_COMMA)
            break;
        lexer_consume_token(parser->lexer, TOKEN_COMMA);
    }

    if (!lexer_consume_token(parser->lexer, TOKEN_RPAREN))
        goto cleanup;

    ast_stmt_t* body = parse_compound_stmt(parser);
    if (body == nullptr)
        goto cleanup;

    fn_def = ast_fn_def_create(id->value, &params, body);

cleanup:
    token_destroy(id);
    ptr_vec_deinit(&params);
    return fn_def;
}

ast_def_t* parse_top_level_definition(parser_t* parser)
{
    switch (lexer_peek_token(parser->lexer)->type)
    {
        case TOKEN_INT:
            return parse_fn_def(parser);
        default:
            break;
    }
    return nullptr;
}

ast_root_t* parser_parse(parser_t* parser)
{
    ptr_vec_t tl_defs = PTR_VEC_INIT;
    ast_def_t* tl_def;
    while ((tl_def = parse_top_level_definition(parser)) != nullptr)
        ptr_vec_append(&tl_defs, tl_def);

    return ast_root_create(&tl_defs);
}
