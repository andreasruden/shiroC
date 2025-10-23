#ifndef AST_DEF_FN_DEF__H
#define AST_DEF_FN_DEF__H

#include "ast/def/def.h"
#include "ast/stmt/stmt.h"
#include "ast/type.h"
#include "common/containers/vec.h"

typedef struct symbol symbol_t;

typedef struct ast_fn_def
{
    ast_def_t base;
    vec_t params;  // vec<ast_param_decl_t>
    ast_type_t* return_type;
    ast_stmt_t* body;
    size_t overload_index; // set by SEMA
    symbol_t* symbol;      // only valid during SEMA (we do not own memory)
    bool exported;
} ast_fn_def_t;

ast_def_t* ast_fn_def_create(const char* name, vec_t* params, ast_type_t* ret_type, ast_stmt_t* body, bool exported);

__attribute__((sentinel))
ast_def_t* ast_fn_def_create_va(const char* name, ast_type_t* ret_type, ast_stmt_t* body, ...);

#endif
