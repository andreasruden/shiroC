#ifndef AST_EXPR_FLOAT_LIT__H
#define AST_EXPR_FLOAT_LIT__H

#include "ast/expr/expr.h"

#include <stdint.h>

typedef struct ast_float_lit
{
    ast_expr_t base;
    double value;
    char* suffix;
} ast_float_lit_t;

ast_expr_t* ast_float_lit_create(double value, const char* suffix);

#endif
