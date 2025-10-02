#ifndef AST_DEF_FN_DEF__H
#define AST_DEF_FN_DEF__H

#include "ast/def/def.h"
#include "ast/stmt/compound_stmt.h"

typedef struct ast_fn_def
{
    ast_def_t base;
    ast_compound_stmt_t* body;
} ast_fn_def_t;

ast_fn_def_t* ast_fn_def_create(const char* name, ast_compound_stmt_t* body);

#endif
