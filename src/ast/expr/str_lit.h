#ifndef AST_EXPR_STR_LIT__H
#define AST_EXPR_STR_LIT__H

#include "ast/expr/expr.h"

#include <stdint.h>

typedef struct ast_str_lit
{
    ast_expr_t base;
    char* value;
} ast_str_lit_t;

ast_expr_t* ast_str_lit_create(const char* value);

#endif
