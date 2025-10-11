#ifndef AST_EXPR_BOOL_LIT__H
#define AST_EXPR_BOOL_LIT__H

#include "ast/expr/expr.h"

#include <stdint.h>

typedef struct ast_bool_lit
{
    ast_expr_t base;
    bool value;
} ast_bool_lit_t;

ast_expr_t* ast_bool_lit_create(bool value);

#endif
