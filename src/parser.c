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
#include "ast/stmt/if_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "ast/stmt/stmt.h"
#include "ast/stmt/while_stmt.h"
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
    compiler_error_t* error = compiler_error_create_for_ast(false, description, ast_node);
    ptr_vec_append(&parser->errors, error);
    return ast_node;
}

static void parser_set_source_tok_to_current(parser_t* parser, void* node, token_t* begin)
{
    source_location_t start_loc, end_loc;
    lexer_get_token_location(parser->lexer, begin, &start_loc);
    lexer_get_current_location(parser->lexer, &end_loc);
    ast_node_set_source(node, &start_loc, &end_loc);
}

// Helper to create an ast_ref_expr_t* with source location filled in
static ast_expr_t* parser_create_ref_expr(parser_t* parser, token_t* id)
{
    ast_expr_t* expr = ast_ref_expr_create(id->value);
    lexer_get_token_location(parser->lexer, id, &AST_NODE(expr)->source_begin);
    lexer_get_token_location(parser->lexer, id, &AST_NODE(expr)->source_end);
    AST_NODE(expr)->source_end.column += (int)strlen(id->value);
    return expr;
}

static ast_expr_t* parse_int_lit(parser_t* parser)
{
    token_t* tok = lexer_next_token_iff(parser->lexer, TOKEN_NUMBER);
    if (tok == nullptr)
        return nullptr;

    int value = atoi(tok->value);
    ast_expr_t* expr = ast_int_lit_create(value);
    parser_set_source_tok_to_current(parser, expr, tok);

    return expr;
}

static ast_expr_t* parse_call_expr(parser_t* parser, token_t* id)
{
    ast_expr_t* call = nullptr;
    ptr_vec_t args = PTR_VEC_INIT;

    if (!lexer_next_token_iff(parser->lexer, TOKEN_LPAREN))
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
        lexer_next_token_iff(parser->lexer, TOKEN_COMMA);
    }

    if (!lexer_next_token_iff(parser->lexer, TOKEN_RPAREN))
        goto cleanup;

    call = ast_call_expr_create(parser_create_ref_expr(parser, id), &args);
    parser_set_source_tok_to_current(parser, call, id);

cleanup:
    ptr_vec_deinit(&args, ast_node_destroy);
    return call;
}

static ast_expr_t* parse_identifier_expr(parser_t* parser)
{
    ast_expr_t* expr = nullptr;

    token_t* id = lexer_next_token_iff(parser->lexer, TOKEN_IDENTIFIER);
    if (id == nullptr)
        goto cleanup;

    if (lexer_peek_token(parser->lexer)->type == TOKEN_LPAREN)
        expr = parse_call_expr(parser, id);
    else
        expr = parser_create_ref_expr(parser, id);

cleanup:
    return expr;
}

static ast_expr_t* parse_paren_expr(parser_t* parser)
{
    token_t* tok_lparen = lexer_next_token_iff(parser->lexer, TOKEN_LPAREN);
    if (tok_lparen == nullptr)
        return nullptr;

    ast_expr_t* expr = parser_parse_expr(parser);
    if (expr == nullptr)
        return nullptr;

    if (!lexer_next_token_iff(parser->lexer, TOKEN_RPAREN))
        return expr; // emit error but yield inner expression

    ast_expr_t* paren_expr = ast_paren_expr_create(expr);
    parser_set_source_tok_to_current(parser, paren_expr, tok_lparen);

    return paren_expr;
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
static ast_expr_t* parser_parse_expr_climb_precedence(parser_t* parser, int min_precedence)
{
    token_t* first_tok = lexer_peek_token(parser->lexer);

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

        ast_expr_t* rhs = parser_parse_expr_climb_precedence(parser,
            token_type_is_right_associative(tok->type) ? precedence : precedence + 1);
        if (rhs == nullptr)
        {
            ast_node_destroy(lhs);
            return nullptr;
        }

        lhs = ast_bin_op_create(tok->type, lhs, rhs);
        parser_set_source_tok_to_current(parser, lhs, first_tok);
    }

    return lhs;
}

ast_expr_t* parser_parse_expr(parser_t* parser)
{
    return parser_parse_expr_climb_precedence(parser, 0);
}

static ast_stmt_t* parse_return_stmt(parser_t* parser)
{
    token_t* tok_return = lexer_next_token_iff(parser->lexer, TOKEN_RETURN);
    if (tok_return == nullptr)
        return nullptr;

    ast_expr_t* expr = parser_parse_expr(parser);
    if (expr == nullptr)
        return nullptr;

    ast_stmt_t* stmt = ast_return_stmt_create(expr);
    lexer_next_token_iff(parser->lexer, TOKEN_SEMICOLON); // this emits error, but keep stmt

    parser_set_source_tok_to_current(parser, stmt, tok_return);

    return stmt;
}

static ast_stmt_t* parse_while_stmt(parser_t* parser)
{
    ast_expr_t* condition = nullptr;
    ast_stmt_t* body = nullptr;

    token_t* tok_while = lexer_next_token_iff(parser->lexer, TOKEN_WHILE);
    if (tok_while == nullptr)
        goto error;

    lexer_next_token_iff(parser->lexer, TOKEN_LPAREN); // emits error

    condition = parser_parse_expr(parser);
    if (condition == nullptr)
        goto error;

    lexer_next_token_iff(parser->lexer, TOKEN_RPAREN); // emits error

    if (lexer_peek_token(parser->lexer)->type != TOKEN_LBRACE)
        lexer_emit_error_for_token(parser->lexer, lexer_peek_token(parser->lexer), TOKEN_LBRACE);

    body = parser_parse_stmt(parser);
    if (body == nullptr)
        goto error;

    ast_stmt_t* stmt = ast_while_stmt_create(condition, body);
    parser_set_source_tok_to_current(parser, stmt, tok_while);
    return stmt;

error:
    ast_node_destroy(condition);
    ast_node_destroy(body);
    return nullptr;
}

static ast_stmt_t* parse_compound_stmt(parser_t* parser)
{
    token_t* tok_lbrace = lexer_next_token_iff(parser->lexer, TOKEN_LBRACE);
    if (tok_lbrace == nullptr)
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

    if (!lexer_next_token_iff(parser->lexer, TOKEN_RBRACE))
    {
        ptr_vec_deinit(&inner_stmts, ast_node_destroy);
        return nullptr;
    }

    ast_stmt_t* stmt = ast_compound_stmt_create(&inner_stmts);
    parser_set_source_tok_to_current(parser, stmt, tok_lbrace);

    return stmt;
}

static ast_decl_t* parse_var_decl(parser_t* parser)
{
    token_t* tok_var = lexer_next_token_iff(parser->lexer, TOKEN_VAR);
    if (tok_var == nullptr)
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
        if (lexer_next_token_iff(parser->lexer, TOKEN_INT))
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

    parser_set_source_tok_to_current(parser, var_decl, tok_var);

    return (ast_decl_t*)var_decl;
}

static ast_stmt_t* parse_decl_stmt(parser_t* parser)
{
    ast_decl_t* decl = parse_var_decl(parser);
    if (decl == nullptr)
        return nullptr;

    ast_stmt_t* stmt = ast_decl_stmt_create(decl);
    lexer_next_token_iff(parser->lexer, TOKEN_SEMICOLON); // do not fail stmt

    source_location_t end_loc;
    lexer_get_current_location(parser->lexer, &end_loc);
    ast_node_set_source_from(stmt, decl, &end_loc);

    return stmt;
}

static ast_stmt_t* parse_expr_stmt(parser_t* parser)
{
    ast_expr_t* expr = parser_parse_expr(parser);
    if (expr == nullptr)
        return nullptr;

    ast_stmt_t* stmt = ast_expr_stmt_create(expr);
    lexer_next_token_iff(parser->lexer, TOKEN_SEMICOLON); // this emits error, but keep stmt

    source_location_t end_loc;
    lexer_get_current_location(parser->lexer, &end_loc);
    ast_node_set_source_from(stmt, expr, &end_loc);

    return stmt;
}

static ast_stmt_t* parse_if_stmt(parser_t* parser)
{
    ast_expr_t* condition = nullptr;
    ast_stmt_t* then_branch = nullptr;
    ast_stmt_t* else_branch = nullptr;

    token_t* tok_if = lexer_next_token_iff(parser->lexer, TOKEN_IF);
    if (tok_if == nullptr)
        goto error;

    lexer_next_token_iff(parser->lexer, TOKEN_LPAREN); // emit error, but continue

    condition = parser_parse_expr(parser);
    if (condition == nullptr)
        goto error;

    lexer_next_token_iff(parser->lexer, TOKEN_RPAREN); // emit error, but continue

    if (lexer_peek_token(parser->lexer)->type != TOKEN_LBRACE)
        lexer_emit_error_for_token(parser->lexer, lexer_peek_token(parser->lexer), TOKEN_LBRACE);

    then_branch = parser_parse_stmt(parser);
    if (then_branch == nullptr)
        goto error;

    if (lexer_peek_token(parser->lexer)->type == TOKEN_ELSE)
    {
        lexer_next_token(parser->lexer);
        if (lexer_peek_token(parser->lexer)->type == TOKEN_IF)
            else_branch = parse_if_stmt(parser);
        else
            else_branch = parser_parse_stmt(parser);

        if (else_branch == nullptr)
            goto error;
    }

    return ast_if_stmt_create(condition, then_branch, else_branch);

error:
    ast_node_destroy(condition);
    ast_node_destroy(then_branch);
    ast_node_destroy(else_branch);
    return nullptr;
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
        case TOKEN_IF:
            return parse_if_stmt(parser);
        case TOKEN_WHILE:
            return parse_while_stmt(parser);
        default:
            return parse_expr_stmt(parser);
    }
}

static ast_decl_t* parse_param_decl(parser_t* parser)
{
    token_t* name_tok = lexer_next_token(parser->lexer);
    lexer_next_token_iff(parser->lexer, TOKEN_COLON);  // emit error, but continue parsing
    token_t* type_tok = lexer_next_token(parser->lexer);

    ast_decl_t* decl = nullptr;
    if (type_tok->type != TOKEN_INT && type_tok->type != TOKEN_IDENTIFIER)
        goto cleanup;
    if (name_tok->type != TOKEN_IDENTIFIER)
        goto cleanup;

    decl = ast_param_decl_create(name_tok->value, type_tok->value);
    parser_set_source_tok_to_current(parser, decl, name_tok);

cleanup:
    return decl;
}

static ast_def_t* parse_fn_def(parser_t* parser)
{
    token_t* id = nullptr;
    ptr_vec_t params = PTR_VEC_INIT;
    ast_def_t* fn_def = nullptr;
    const char* ret_type = nullptr;

    token_t* tok_fn = lexer_next_token_iff(parser->lexer, TOKEN_FN);
    if (!tok_fn)
        goto cleanup;

    id = lexer_next_token_iff(parser->lexer, TOKEN_IDENTIFIER);
    if (id == nullptr)
        goto cleanup;

    if (!lexer_next_token_iff(parser->lexer, TOKEN_LPAREN))
        goto cleanup;

    // Parameters
    token_type_t type;
    while ((type = lexer_peek_token(parser->lexer)->type) != TOKEN_EOF && type != TOKEN_RPAREN)
    {
        ast_decl_t* param = parse_param_decl(parser);
        if (param == nullptr)
            goto cleanup;
        ptr_vec_append(&params, param);

        if (lexer_peek_token(parser->lexer)->type != TOKEN_COMMA)
            break;
        lexer_next_token_iff(parser->lexer, TOKEN_COMMA);
    }

    if (!lexer_next_token_iff(parser->lexer, TOKEN_RPAREN))
        goto cleanup;

    // Optional return type
    if (lexer_peek_token(parser->lexer)->type == TOKEN_ARROW)
    {
        lexer_next_token(parser->lexer);
        // TODO: Handle more types
        if (lexer_next_token_iff(parser->lexer, TOKEN_INT))  // accept error
            ret_type = "int";
    }

    // Body
    ast_stmt_t* body = parse_compound_stmt(parser);
    if (body == nullptr)
        goto cleanup;

    fn_def = ast_fn_def_create(id->value, &params, ret_type, body);
    parser_set_source_tok_to_current(parser, fn_def, tok_fn);

cleanup:
    ptr_vec_deinit(&params, ast_node_destroy);
    return fn_def;
}

static ast_def_t* parse_top_level_definition(parser_t* parser)
{
    switch (lexer_peek_token(parser->lexer)->type)
    {
        case TOKEN_FN:
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

    // We're currently not setting source for root, but it also seems kind of pointless
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
