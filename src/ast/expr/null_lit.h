#ifndef AST_EXPR_NULL_LIT__H
#define AST_EXPR_NULL_LIT__H

#include "ast/expr/expr.h"

#include <stdint.h>

typedef struct ast_null_lit
{
    ast_expr_t base;
} ast_null_lit_t;

ast_expr_t* ast_null_lit_create();

#endif
