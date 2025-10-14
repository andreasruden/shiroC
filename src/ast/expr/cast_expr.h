#ifndef AST_EXPR_CAST__H
#define AST_EXPR_CAST__H

#include "ast/expr/expr.h"

typedef struct ast_cast_expr
{
    ast_expr_t base;
    ast_expr_t* expr;
    ast_type_t* target;
} ast_cast_expr_t;

ast_expr_t* ast_cast_expr_create(ast_expr_t* expr, ast_type_t* target);

#endif
