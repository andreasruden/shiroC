#ifndef AST_EXPR_INT_LIT__H
#define AST_EXPR_INT_LIT__H

#include "ast/expr/expr.h"

#include <stdint.h>

typedef struct ast_int_lit
{
    ast_expr_t base;
    int64_t value;
} ast_int_lit_t;

ast_expr_t* ast_int_lit_create(int64_t value);

#endif
