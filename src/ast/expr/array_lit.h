#ifndef AST_EXPR_ARRAY_LIT__H
#define AST_EXPR_ARRAY_LIT__H

#include "ast/expr/expr.h"

#include <stdint.h>

typedef struct ast_array_lit
{
    ast_expr_t base;
    vec_t exprs;
} ast_array_lit_t;

// Ownership of exprs is transferred to ast_array_lit
ast_expr_t* ast_array_lit_create(vec_t* exprs);

__attribute__((sentinel))
ast_expr_t* ast_array_lit_create_va(ast_expr_t* first, ...);

ast_expr_t* ast_array_lit_create_empty();

#endif
