#ifndef AST_EXPR_UNARY_OP__H
#define AST_EXPR_UNARY_OP__H

#include "ast/expr/expr.h"
#include "common/containers/vec.h"
#include "parser/lexer.h"

typedef struct ast_unary_op
{
    ast_expr_t base;
    token_type_t op;
    ast_expr_t* expr;
} ast_unary_op_t;

ast_expr_t* ast_unary_op_create(token_type_t op, ast_expr_t* expr);

#endif
