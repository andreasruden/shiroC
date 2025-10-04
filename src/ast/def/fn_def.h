#ifndef AST_DEF_FN_DEF__H
#define AST_DEF_FN_DEF__H

#include "ast/def/def.h"
#include "ast/stmt/stmt.h"

typedef struct ast_fn_def
{
    ast_def_t base;
    ast_stmt_t* body;
} ast_fn_def_t;

ast_def_t* ast_fn_def_create(const char* name, ast_stmt_t* body);

#endif
