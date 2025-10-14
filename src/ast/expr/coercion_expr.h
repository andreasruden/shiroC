#ifndef AST_EXPR_COERCION__H
#define AST_EXPR_COERCION__H

#include "ast/expr/expr.h"

typedef struct ast_coercion_expr
{
    ast_expr_t base;
    ast_expr_t* expr;
    ast_type_t* target;
} ast_coercion_expr_t;

ast_expr_t* ast_coercion_expr_create(ast_expr_t* expr, ast_type_t* target);

#endif
