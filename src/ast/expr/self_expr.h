#ifndef AST_EXPR_SELF_EXPR__H
#define AST_EXPR_SELF_EXPR__H

#include "ast/expr/expr.h"

#include <stdbool.h>

typedef struct ast_self_expr
{
    ast_expr_t base;
    bool implicit;
} ast_self_expr_t;

ast_expr_t* ast_self_expr_create(bool implicit);

#endif
