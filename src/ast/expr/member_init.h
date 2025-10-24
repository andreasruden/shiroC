#ifndef AST_EXPR_MEMBER_INIT__H
#define AST_EXPR_MEMBER_INIT__H

#include "ast/expr/expr.h"

// NOTE: This is not an expression, but it doesn't really fit into any category...
typedef struct ast_member_init
{
    ast_node_t base;
    char* member_name;
    ast_expr_t* init_expr;
    ast_type_t* class_type;  // nullptr until resolved by SEMA
} ast_member_init_t;

ast_member_init_t* ast_member_init_create(const char* member_name, ast_expr_t* init_expr);

#endif
