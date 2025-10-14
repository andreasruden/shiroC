#include "parser.h"

#include "ast/decl/param_decl.h"
#include "ast/decl/var_decl.h"
#include "ast/def/def.h"
#include "ast/def/fn_def.h"
#include "ast/expr/array_lit.h"
#include "ast/expr/array_subscript.h"
#include "ast/expr/bin_op.h"
#include "ast/expr/bool_lit.h"
#include "ast/expr/call_expr.h"
#include "ast/expr/expr.h"
#include "ast/expr/float_lit.h"
#include "ast/expr/int_lit.h"
#include "ast/expr/null_lit.h"
#include "ast/expr/paren_expr.h"
#include "ast/expr/ref_expr.h"
#include "ast/expr/str_lit.h"
#include "ast/expr/unary_op.h"
#include "ast/node.h"
#include "ast/root.h"
#include "ast/stmt/compound_stmt.h"
#include "ast/stmt/decl_stmt.h"
#include "ast/stmt/expr_stmt.h"
#include "ast/stmt/if_stmt.h"
#include "ast/stmt/return_stmt.h"
#include "ast/stmt/stmt.h"
#include "ast/stmt/while_stmt.h"
#include "ast/type.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "common/util/ssprintf.h"
#include "compiler_error.h"
#include "lexer.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

parser_t* parser_create()
{
    parser_t* parser = malloc(sizeof(*parser));

    *parser = (parser_t){
        .lex_errors = VEC_INIT(compiler_error_destroy_void),
        .errors = VEC_INIT(nullptr),  // mix of lex errors and AST node errors
    };

    // Dummy lexer, will be replaced when source is set
    parser->lexer = lexer_create("", "", nullptr, parser);

    return parser;
}

void parser_destroy(parser_t* parser)
{
    if (parser == nullptr)
        return;

    lexer_destroy(parser->lexer);
    vec_deinit(&parser->lex_errors);
    vec_deinit(&parser->errors);
    free(parser);
}

static void parser_on_lex_error(compiler_error_t* error, void* arg)
{
    parser_t* parser = arg;
    vec_push(&parser->lex_errors, error);
    vec_push(&parser->errors, error);
}

static void parser_error(parser_t* parser, void* ast_node, const char* description)
{
    compiler_error_t* error = compiler_error_create_for_ast(false, description, ast_node);
    vec_push(&parser->errors, error);
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

static ast_expr_t* parse_array_lit(parser_t* parser)
{
    vec_t exprs = VEC_INIT(ast_node_destroy);

    if (!lexer_next_token_iff(parser->lexer, TOKEN_LBRACKET))
        goto error;

    while (true)
    {
        ast_expr_t* expr = parser_parse_expr(parser);
        if (expr == nullptr)
            goto error;
        vec_push(&exprs, expr);

        if (lexer_peek_token(parser->lexer)->type != TOKEN_COMMA)
            break;
        lexer_next_token(parser->lexer);
    }

    lexer_next_token_iff(parser->lexer, TOKEN_RBRACKET);  // emit error, but return node

    return ast_array_lit_create(&exprs);  // ownership transfererd

error:
    vec_deinit(&exprs);
    return nullptr;
}

static ast_expr_t* parse_bool_lit(parser_t* parser)
{
    token_t* tok = lexer_peek_token(parser->lexer);

    if (tok->type != TOKEN_FALSE && tok->type != TOKEN_TRUE)
    {
        lexer_emit_error_for_token(parser->lexer, tok, TOKEN_UNKNOWN);
        return nullptr;
    }

    lexer_next_token(parser->lexer);
    bool value = tok->type == TOKEN_TRUE;
    ast_expr_t* expr = ast_bool_lit_create(value);
    parser_set_source_tok_to_current(parser, expr, tok);

    return expr;
}

static ast_expr_t* parse_float_lit(parser_t* parser)
{
    token_t* tok = lexer_next_token_iff(parser->lexer, TOKEN_FLOAT);
    if (tok == nullptr)
        return nullptr;

    errno = 0;
    char* endptr;
    double value = strtod(tok->value, &endptr);
    ast_expr_t* expr = ast_float_lit_create(value, tok->suffix);
    parser_set_source_tok_to_current(parser, expr, tok);

    if (errno != 0)
        parser_error(parser, expr, ssprintf("strtod failed for input %s", tok->value));

    // Sanity check, but should not be possible if lexer does not have a bug:
    panic_if(endptr == tok->value);
    panic_if(*endptr != '\0');

    return expr;
}

static ast_expr_t* parse_int_lit(parser_t* parser)
{
    token_t* tok = lexer_next_token_iff(parser->lexer, TOKEN_INTEGER);
    if (tok == nullptr)
        return nullptr;

    bool has_minus_sign = tok->value[0] == '-';
    errno = 0;
    char* endptr;
    const char* num_start = has_minus_sign ? tok->value + 1 : tok->value;
    uint64_t magnitude = strtoull(num_start, &endptr, 0);

    bool range_err = errno == ERANGE;

    if (endptr == tok->value || *endptr != '\0')
    {
        lexer_emit_token_malformed(parser->lexer, tok, "invalid integer literal");
        return nullptr;
    }

    ast_expr_t* expr = ast_int_lit_create(has_minus_sign, magnitude, tok->suffix);
    parser_set_source_tok_to_current(parser, expr, tok);
    if (range_err)
        parser_error(parser, expr, ssprintf("integer literal value '%s' is too large", tok->value));

    return expr;
}

static ast_expr_t* parse_null_lit(parser_t* parser)
{
    token_t* tok = lexer_next_token_iff(parser->lexer, TOKEN_NULL);
    if (tok == nullptr)
        return nullptr;

    ast_expr_t* expr = ast_null_lit_create();
    parser_set_source_tok_to_current(parser, expr, tok);

    return expr;
}

static ast_expr_t* parse_str_lit(parser_t* parser)
{
    token_t* tok = lexer_next_token_iff(parser->lexer, TOKEN_STRING);
    if (tok == nullptr)
        return nullptr;

    ast_expr_t* expr = ast_str_lit_create(tok->value);
    parser_set_source_tok_to_current(parser, expr, tok);

    return expr;
}

static ast_expr_t* parse_array_subscript(parser_t* parser, token_t* id)
{
    if (!lexer_next_token_iff(parser->lexer, TOKEN_LBRACKET))
        return nullptr;

    ast_expr_t* index = parser_parse_expr(parser);
    if (index == nullptr)
        return nullptr;

    if (!lexer_next_token_iff(parser->lexer, TOKEN_RBRACKET))
        return nullptr;

    return ast_array_subscript_create(ast_ref_expr_create(id->value), index);
}

static ast_expr_t* parse_call_expr(parser_t* parser, token_t* id)
{
    ast_expr_t* call = nullptr;
    vec_t args = VEC_INIT(ast_node_destroy);

    if (!lexer_next_token_iff(parser->lexer, TOKEN_LPAREN))
        goto cleanup;

    token_type_t type;
    while ((type = lexer_peek_token(parser->lexer)->type) != TOKEN_EOF && type != TOKEN_RPAREN)
    {
        ast_expr_t* arg = parser_parse_primary_expr(parser);
        if (arg == nullptr)
            goto cleanup;
        vec_push(&args, arg);

        if (lexer_peek_token(parser->lexer)->type != TOKEN_COMMA)
            break;
        lexer_next_token_iff(parser->lexer, TOKEN_COMMA);
    }

    if (!lexer_next_token_iff(parser->lexer, TOKEN_RPAREN))
        goto cleanup;

    call = ast_call_expr_create(parser_create_ref_expr(parser, id), &args);
    parser_set_source_tok_to_current(parser, call, id);

cleanup:
    vec_deinit(&args);
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
    else if (lexer_peek_token(parser->lexer)->type == TOKEN_LBRACKET)
        expr = parse_array_subscript(parser, id);
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
        case TOKEN_FLOAT:
            return parse_float_lit(parser);
        case TOKEN_INTEGER:
            return parse_int_lit(parser);
        case TOKEN_IDENTIFIER:
            return parse_identifier_expr(parser);
        case TOKEN_LPAREN:
            return parse_paren_expr(parser);
        case TOKEN_TRUE:
        case TOKEN_FALSE:
            return parse_bool_lit(parser);
        case TOKEN_STRING:
            return parse_str_lit(parser);
        case TOKEN_NULL:
            return parse_null_lit(parser);
        case TOKEN_LBRACKET:
            return parse_array_lit(parser);
        default:
            break;
    }
    return nullptr;
}

static ast_expr_t* parse_unary_expr(parser_t* parser)
{
    token_t* tok = lexer_peek_token(parser->lexer);

    if (token_type_is_unary_op(tok->type))
    {
        lexer_next_token(parser->lexer);

        // Recursively parse unary to handle chains like *&x
        ast_expr_t* inner_expr = parse_unary_expr(parser);
        if (inner_expr == nullptr)
            return nullptr;

        ast_expr_t* expr = ast_unary_op_create(tok->type, inner_expr);
        parser_set_source_tok_to_current(parser, expr, tok);
        return expr;
    }

    return parser_parse_primary_expr(parser);
}

// Use precedence climbing to efficiently parse binary operator expressions
static ast_expr_t* parser_parse_expr_climb_precedence(parser_t* parser, int min_precedence)
{
    token_t* first_tok = lexer_peek_token(parser->lexer);

    ast_expr_t* lhs = parse_unary_expr(parser);
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

    vec_t inner_stmts = VEC_INIT(ast_node_destroy);
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
        vec_push(&inner_stmts, inner_stmt);
    }

    if (!lexer_next_token_iff(parser->lexer, TOKEN_RBRACE))
    {
        vec_deinit(&inner_stmts);
        return nullptr;
    }

    ast_stmt_t* stmt = ast_compound_stmt_create(&inner_stmts);
    parser_set_source_tok_to_current(parser, stmt, tok_lbrace);

    return stmt;
}

static ast_type_t* parse_type_annotation_array(parser_t* parser);
static ast_type_t* parse_type_annotation_view(parser_t* parser);

static ast_type_t* parse_type_annotation(parser_t* parser)
{
    token_t* type_tok = lexer_peek_token(parser->lexer);

    // Fundamental type
    ast_type_t* type;
    if (type_tok->type == TOKEN_LBRACKET)
        type = parse_type_annotation_array(parser);
    else if (type_tok->type == TOKEN_VIEW)
        type = parse_type_annotation_view(parser);
    else
    {
        type = ast_type_from_token(type_tok);
        lexer_next_token(parser->lexer);
    }

    if (type->kind == AST_TYPE_INVALID)
    {
        lexer_emit_error_for_token(parser->lexer, type_tok, TOKEN_IDENTIFIER);
    }
    else
    {
        // Consume pointer wrappers
        while (lexer_peek_token(parser->lexer)->type == TOKEN_STAR)
        {
            lexer_next_token(parser->lexer);
            type = ast_type_pointer(type);
        }
    }

    return type;
}

static ast_type_t* parse_type_annotation_array(parser_t* parser)
{
    // [
    if (!lexer_next_token_iff(parser->lexer, TOKEN_LBRACKET))
        return ast_type_invalid();

    // T
    ast_type_t* element_type = parse_type_annotation(parser);

    // , size
    ast_type_t* array_type;
    if (lexer_peek_token(parser->lexer)->type == TOKEN_COMMA)
    {
        lexer_next_token(parser->lexer);
        ast_expr_t* size_expr = parser_parse_expr(parser);
        if (size_expr == nullptr)
            return ast_type_invalid();
        array_type = ast_type_array_size_unresolved(element_type, size_expr);
    }
    else
        array_type = ast_type_heap_array(element_type);

    // ]
    if (!lexer_next_token_iff(parser->lexer, TOKEN_RBRACKET))
        return ast_type_invalid();

    return array_type;
}

static ast_type_t* parse_type_annotation_view(parser_t* parser)
{
    // view
    if (!lexer_next_token_iff(parser->lexer, TOKEN_VIEW))
        return ast_type_invalid();

    // [
    if (!lexer_next_token_iff(parser->lexer, TOKEN_LBRACKET))
        return ast_type_invalid();

    // T
    ast_type_t* element_type = parse_type_annotation(parser);

    // ]
    if (!lexer_next_token_iff(parser->lexer, TOKEN_RBRACKET))
        return ast_type_invalid();

    return ast_type_view(element_type);
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

    // Optional type annotation
    if (lexer_peek_token(parser->lexer)->type == TOKEN_COLON)
    {
        lexer_next_token(parser->lexer);
        var_decl->type = parse_type_annotation(parser);
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

    ast_stmt_t* if_stmt = ast_if_stmt_create(condition, then_branch, else_branch);
    parser_set_source_tok_to_current(parser, if_stmt, tok_if);
    return if_stmt;

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
    token_t* name_tok = lexer_next_token_iff(parser->lexer, TOKEN_IDENTIFIER);
    if (name_tok == nullptr)
        return nullptr;

    lexer_next_token_iff(parser->lexer, TOKEN_COLON);  // emit error, but continue parsing

    token_t* type_tok = lexer_peek_token(parser->lexer);
    ast_type_t* type = ast_type_from_token(type_tok);
    if (type->kind == AST_TYPE_INVALID)
    {
        lexer_emit_error_for_token(parser->lexer, type_tok, TOKEN_IDENTIFIER);
        return nullptr;
    }
    lexer_next_token(parser->lexer);

    ast_decl_t* decl = ast_param_decl_create(name_tok->value, type);
    parser_set_source_tok_to_current(parser, decl, name_tok);

    return decl;
}

static ast_def_t* parse_fn_def(parser_t* parser)
{
    token_t* id = nullptr;
    vec_t params = VEC_INIT(ast_node_destroy);
    ast_def_t* fn_def = nullptr;
    ast_type_t* ret_type = nullptr;

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
        vec_push(&params, param);

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
        ret_type = ast_type_from_token(lexer_next_token(parser->lexer));
    }

    // Body
    ast_stmt_t* body = parse_compound_stmt(parser);
    if (body == nullptr)
        goto cleanup;

    fn_def = ast_fn_def_create(id->value, &params, ret_type, body);
    parser_set_source_tok_to_current(parser, fn_def, tok_fn);

    if (ret_type != nullptr && ret_type->kind == AST_TYPE_INVALID)
        parser_error(parser, fn_def, "missing return type after '->'");
    ret_type = nullptr;

cleanup:
    vec_deinit(&params);
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
    token_t* first = lexer_peek_token(parser->lexer);
    vec_t tl_defs = VEC_INIT(ast_node_destroy);

    token_t* next_token;
    while ((next_token = lexer_peek_token(parser->lexer))->type != TOKEN_EOF)
    {
        ast_def_t* tl_def = parse_top_level_definition(parser);
        if (tl_def == nullptr)
        {
            // Consume the faulty token and try again:
            if (next_token == lexer_peek_token(parser->lexer))
            {
                lexer_emit_error_for_token(parser->lexer, next_token, TOKEN_UNKNOWN);
                lexer_next_token(parser->lexer);
            }
        }
        else
            vec_push(&tl_defs, tl_def);
    }

    ast_root_t* root = ast_root_create(&tl_defs);
    parser_set_source_tok_to_current(parser, root, first);
    return root;
}

void parser_set_source(parser_t* parser, const char* filename, const char* source)
{
    parser_reset(parser);
    parser->lexer = lexer_create(filename, source, parser_on_lex_error, parser);
}

void parser_reset(parser_t* parser)
{
    lexer_destroy(parser->lexer);
    vec_deinit(&parser->lex_errors);
    parser->lex_errors = VEC_INIT(compiler_error_destroy_void);
    vec_deinit(&parser->errors);
    parser->errors = VEC_INIT(nullptr);
}

vec_t* parser_errors(parser_t* parser)
{
    return &parser->errors;
}
