#ifndef AST_DECL_MEMBER_DECL__H
#define AST_DECL_MEMBER_DECL__H

#include "ast/decl/var_decl.h"

typedef struct ast_member_decl
{
    ast_var_decl_t base;
} ast_member_decl_t;

ast_decl_t* ast_member_decl_create_from(ast_var_decl_t* var);

ast_decl_t* ast_member_decl_create(const char* name, ast_type_t* type, ast_expr_t* init_expr);

#endif
