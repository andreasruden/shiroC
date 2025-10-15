#ifndef AST_EXPR_UNINIT_LIT__H
#define AST_EXPR_UNINIT_LIT__H

#include "ast/expr/expr.h"

#include <stdint.h>

typedef struct ast_uninit_lit
{
    ast_expr_t base;
} ast_uninit_lit_t;

ast_expr_t* ast_uninit_lit_create();

#endif
