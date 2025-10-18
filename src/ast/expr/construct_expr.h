#ifndef AST_EXPR_CONSTRUCT__H
#define AST_EXPR_CONSTRUCT__H

#include "ast/expr/expr.h"

typedef struct ast_construct_expr
{
    ast_expr_t base;
    ast_type_t* class_type;
    vec_t member_inits;  // ast_member_init_t*
} ast_construct_expr_t;

// Ownership of member_inits transferred
ast_expr_t* ast_construct_expr_create(ast_type_t* class_type, vec_t* member_inits);

__attribute__((sentinel))
ast_expr_t* ast_construct_expr_create_va(ast_type_t* class_type, ...);

#endif
