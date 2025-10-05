#ifndef AST_VAR_DECL__H
#define AST_VAR_DECL__H

#include "ast/decl/decl.h"
#include "ast/expr/expr.h"

typedef struct ast_var_decl
{
    ast_decl_t base;
    char* name;
    char* type;             // can be nullptr
    ast_expr_t* init_expr;  // can be nullptr
} ast_var_decl_t;

ast_decl_t* ast_var_decl_create(const char* name, const char* type, ast_expr_t* init_expr);

ast_var_decl_t* ast_var_decl_create_mandatory(const char* name);

#endif
