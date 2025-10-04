#ifndef AST_DEF_FN_DEF__H
#define AST_DEF_FN_DEF__H

#include "ast/def/def.h"
#include "ast/stmt/stmt.h"
#include "common/containers/ptr_vec.h"

typedef struct ast_fn_def
{
    ast_def_t base;
    ptr_vec_t params;  // ptr_vec<ast_param_decl_t>
    ast_stmt_t* body;
} ast_fn_def_t;

ast_def_t* ast_fn_def_create(const char* name, ptr_vec_t* params, ast_stmt_t* body);

__attribute__((sentinel))
ast_def_t* ast_fn_def_create_va(const char* name, ast_stmt_t* body, ...);

#endif
