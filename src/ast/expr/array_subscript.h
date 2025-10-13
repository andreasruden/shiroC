#ifndef AST_EXPR_ARRAY_SUBSCRIPT_H
#define AST_EXPR_ARRAY_SUBSCRIPT_H

#include "ast/expr/expr.h"

#include <stdint.h>

typedef struct ast_array_subscript
{
    ast_expr_t base;
    ast_expr_t* array;
    ast_expr_t* index;
} ast_array_subscript_t;

ast_expr_t* ast_array_subscript_create(ast_expr_t* array, ast_expr_t* index);

#endif
