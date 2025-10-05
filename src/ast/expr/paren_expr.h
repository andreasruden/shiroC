#ifndef AST_EXPR_PAREN__H
#define AST_EXPR_PAREN__H

#include "ast/expr/expr.h"

typedef struct ast_paren_expr
{
    ast_expr_t base;
    ast_expr_t* expr;
} ast_paren_expr_t;

ast_expr_t* ast_paren_expr_create(ast_expr_t* expr);

#endif
