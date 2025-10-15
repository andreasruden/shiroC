#ifndef AST_EXPR_ARRAY_SLICE_H
#define AST_EXPR_ARRAY_SLICE_H

#include "ast/expr/expr.h"

typedef struct ast_array_slice
{
    ast_expr_t base;
    ast_expr_t* array;
    ast_expr_t* start;      // Start index, inclusive (nullable for [..end])
    ast_expr_t* end;        // End index, exclusive (nullable for [start..])
} ast_array_slice_t;

ast_expr_t* ast_array_slice_create(ast_expr_t* array, ast_expr_t* start, ast_expr_t* end);

#endif
