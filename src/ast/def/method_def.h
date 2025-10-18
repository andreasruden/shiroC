#ifndef AST_DEF_METHOD_DEF__H
#define AST_DEF_METHOD_DEF__H

#include "ast/def/fn_def.h"

typedef struct ast_method_def
{
    ast_fn_def_t base;
} ast_method_def_t;

// Ownership of fn_def transferred
ast_def_t* ast_method_def_create_from(ast_fn_def_t* fn_def);

ast_def_t* ast_method_def_create(const char* name, vec_t* params, ast_type_t* ret_type, ast_stmt_t* body);

__attribute__((sentinel))
ast_def_t* ast_method_def_create_va(const char* name, ast_type_t* ret_type, ast_stmt_t* body, ...);

#endif
