#ifndef AST_STMT_DECL__H
#define AST_STMT_DECL__H

#include "ast/decl/decl.h"
#include "ast/stmt/stmt.h"

typedef struct ast_decl_stmt
{
    ast_stmt_t base;
    ast_decl_t* decl;
} ast_decl_stmt_t;

// The decls memory ownership is transferred
ast_stmt_t* ast_decl_stmt_create(ast_decl_t* decl);

#endif
