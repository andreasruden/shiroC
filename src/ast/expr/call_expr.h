#ifndef AST_EXPR_CALL__H
#define AST_EXPR_CALL__H

#include "ast/expr/expr.h"
#include "common/containers/ptr_vec.h"

typedef struct ast_call_expr
{
    ast_expr_t base;
    ast_expr_t* function;
    ptr_vec_t arguments; // ptr_vec<ast_expr_t*>
} ast_call_expr_t;

ast_expr_t* ast_call_expr_create(ast_expr_t* function, ptr_vec_t* arguments);

__attribute__((sentinel))
ast_expr_t* ast_call_expr_create_va(ast_expr_t* function, ...);

#endif
