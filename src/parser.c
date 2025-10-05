#include "parser.h"

#include "ast/decl/param_decl.h"
#include "ast/decl/var_decl.h"
#include "ast/def/def.h"
#include "ast/def/fn_def.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/call_expr.h"
#include "ast/expr/expr.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/paren_expr.h"
#include "ast/expr/ref_expr.h"
#include "ast/node.h"
#include "ast/root.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/stmt/expr_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "common/containers/ptr_vec.h"
#include "compiler_error.h"
#include "lexer.h"

#include <stdlib.h>
#include <string.h>

parser_t* parser_create()
{
    parser_t* parser = malloc(sizeof(*parser));

    *parser = (parser_t){
        .errors = PTR_VEC_INIT,
    };

    return parser;
}

void parser_destroy(parser_t* parser)
{
    if (parser != nullptr)
    {
        lexer_destroy(parser->lexer);
        ptr_vec_deinit(&parser->errors, compiler_error_destroy_void);
        free(parser);
    }
}

static void* parser_error(parser_t* parser, void* ast_node, const char* description)
{
    compiler_error_t* error = compiler_error_create(false, ast_node, description, parser->lexer->filename, 0, 0);
    ptr_vec_append(&parser->errors, error);
    return ast_node;
}

ast_expr_t* parse_int_lit(parser_t* parser)
{
    token_t* tok = lexer_next_token_iff(parser->lexer, TOKEN_NUMBER);
    if (tok == nullptr)
        return nullptr;

    int value = atoi(tok->value);
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
        ast_expr_t* arg = parser_parse_primary_expr(parser);
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
    ptr_vec_deinit(&args, ast_node_destroy);
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
    return expr;
}

ast_expr_t* parse_paren_expr(parser_t* parser)
{
    if (!lexer_consume_token(parser->lexer, TOKEN_LPAREN))
        return nullptr;

    ast_expr_t* expr = parser_parse_expr(parser);
    if (expr == nullptr)
        return nullptr;

    if (!lexer_consume_token(parser->lexer, TOKEN_RPAREN))
        return expr; // emit error but yield inner expression

    return ast_paren_expr_create(expr);
}

ast_expr_t* parser_parse_primary_expr(parser_t* parser)
{
    switch (lexer_peek_token(parser->lexer)->type)
    {
        case TOKEN_NUMBER:
            return parse_int_lit(parser);
        case TOKEN_IDENTIFIER:
            return parse_identifier_expr(parser);
        case TOKEN_LPAREN:
            return parse_paren_expr(parser);
        default:
            break;
    }
    return nullptr;
}

// Use precedence climbing to efficiently parse expressions
ast_expr_t* parser_parse_expr_climb_precedence(parser_t* parser, int min_precedence)
{
    ast_expr_t* lhs = parser_parse_primary_expr(parser);
    if (lhs == nullptr)
        return nullptr;

    token_t* tok;
    while (token_type_is_bin_op((tok = lexer_peek_token(parser->lexer))->type))
    {
        int precedence = token_type_get_precedence(tok->type);
        if (precedence < min_precedence)
            break;

        lexer_next_token(parser->lexer);  // consume token

        ast_expr_t* rhs = parser_parse_expr_climb_precedence(parser, precedence + 1);
        if (rhs == nullptr)
        {
            ast_node_destroy(lhs);
            return nullptr;
        }

        lhs = ast_bin_op_create(tok->type, lhs, rhs);
    }

    return lhs;
}

ast_expr_t* parser_parse_expr(parser_t* parser)
{
    return parser_parse_expr_climb_precedence(parser, 0);
}

ast_stmt_t* parse_return_stmt(parser_t* parser)
{
    if (!lexer_consume_token(parser->lexer, TOKEN_RETURN))
        return nullptr;

    ast_expr_t* expr = parser_parse_expr(parser);
    if (expr == nullptr)
        return nullptr;

    ast_stmt_t* stmt = ast_return_stmt_create(expr);
    lexer_consume_token(parser->lexer, TOKEN_SEMICOLON); // this emits error, but keep stmt
    return stmt;
}

ast_stmt_t* parse_compound_stmt(parser_t* parser)
{
    if (!lexer_consume_token(parser->lexer, TOKEN_LBRACE))
        return nullptr;

    ptr_vec_t inner_stmts = PTR_VEC_INIT;
    token_t* tok;
    while ((tok = lexer_peek_token(parser->lexer))->type != TOKEN_EOF && tok->type != TOKEN_RBRACE)
    {
        ast_stmt_t* inner_stmt = parser_parse_stmt(parser);
        if (inner_stmt == nullptr)
        {
            if (tok == lexer_peek_token(parser->lexer))
                lexer_next_token(parser->lexer);  // consume faulty token
            continue;  // don't add faulty sub-statement, but don't abort
        }
        ptr_vec_append(&inner_stmts, inner_stmt);
    }

    if (!lexer_consume_token(parser->lexer, TOKEN_RBRACE))
    {
        ptr_vec_deinit(&inner_stmts, ast_node_destroy);
        return nullptr;
    }

    return ast_compound_stmt_create(&inner_stmts);
}

ast_decl_t* parse_var_decl(parser_t* parser)
{
    if (!lexer_consume_token(parser->lexer, TOKEN_VAR))
        return nullptr;

    token_t* name = lexer_next_token_iff(parser->lexer, TOKEN_IDENTIFIER);
    if (name == nullptr)
        return nullptr;

    ast_var_decl_t* var_decl = ast_var_decl_create_mandatory(name->value);

    // Optional type specification
    if (lexer_peek_token(parser->lexer)->type == TOKEN_COLON)
    {
        lexer_next_token(parser->lexer);
        // TODO: Handle user-defined types, pointers, etc
        if (lexer_consume_token(parser->lexer, TOKEN_INT))
            var_decl->type = strdup("int");
    }

    // Optional initialization expression
    ast_expr_t* init_expr = nullptr;
    if (lexer_peek_token(parser->lexer)->type == TOKEN_ASSIGN)
    {
        lexer_next_token(parser->lexer);
        init_expr = parser_parse_expr(parser);
        var_decl->init_expr = init_expr;
    }

    if (var_decl->type == nullptr && var_decl->init_expr == nullptr)
        parser_error(parser, var_decl, "variable declaration must have either a type annotation or an initializer");
    else if (var_decl->type != nullptr && var_decl->init_expr != nullptr)
        parser_error(parser, var_decl, "variable declaration cannot have both a type annotation and an initializer");

    return (ast_decl_t*)var_decl;
}

ast_stmt_t* parse_decl_stmt(parser_t* parser)
{
    ast_decl_t* decl = parse_var_decl(parser);
    if (decl == nullptr)
        return nullptr;
    ast_stmt_t* stmt = ast_decl_stmt_create(decl);
    lexer_consume_token(parser->lexer, TOKEN_SEMICOLON); // do not fail stmt
    return stmt;
}

ast_stmt_t* parse_expr_stmt(parser_t* parser)
{
    ast_expr_t* expr = parser_parse_expr(parser);
    if (expr == nullptr)
        return nullptr;

    ast_stmt_t* stmt = ast_expr_stmt_create(expr);
    lexer_consume_token(parser->lexer, TOKEN_SEMICOLON); // this emits error, but keep stmt
    return stmt;
}

ast_stmt_t* parser_parse_stmt(parser_t* parser)
{
    switch (lexer_peek_token(parser->lexer)->type)
    {
        case TOKEN_RETURN:
            return parse_return_stmt(parser);
        case TOKEN_LBRACE:
            return parse_compound_stmt(parser);
        case TOKEN_VAR:
            return parse_decl_stmt(parser);
        default:
            return parse_expr_stmt(parser);
    }
}

ast_decl_t* parse_param_decl(parser_t* parser)
{
    token_t* type_tok = lexer_next_token(parser->lexer);
    token_t* name_tok = lexer_next_token(parser->lexer);

    ast_decl_t* decl = nullptr;
    if (type_tok->type != TOKEN_INT && type_tok->type != TOKEN_IDENTIFIER)
        goto cleanup;
    if (name_tok->type != TOKEN_IDENTIFIER)
        goto cleanup;
    decl = ast_param_decl_create(type_tok->value, name_tok->value);

cleanup:
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
        ast_decl_t* param = parse_param_decl(parser);
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
    ptr_vec_deinit(&params, ast_node_destroy);
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

    token_t* next_token;
    while ((next_token = lexer_peek_token(parser->lexer))->type != TOKEN_EOF)
    {
        ast_def_t* tl_def = parse_top_level_definition(parser);
        if (tl_def == nullptr)
        {
            // Consume the faulty token and try again:
            if (next_token == lexer_peek_token(parser->lexer))
                lexer_next_token(parser->lexer);
        }
        else
            ptr_vec_append(&tl_defs, tl_def);
    }

    return ast_root_create(&tl_defs);
}

void parser_set_source(parser_t* parser, const char* filename, const char* source)
{
    parser_reset(parser);
    parser->lexer = lexer_create(filename, source, &parser->errors);
}

void parser_reset(parser_t* parser)
{
    lexer_destroy(parser->lexer);
    ptr_vec_deinit(&parser->errors, compiler_error_destroy_void);
    parser->errors = PTR_VEC_INIT;
}

ptr_vec_t* parser_errors(parser_t* parser)
{
    return &parser->errors;
}
