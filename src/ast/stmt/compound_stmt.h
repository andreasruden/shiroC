#ifndef AST_STMT_COMPOUND_STMT__H
#define AST_STMT_COMPOUND_STMT__H

#include "ast/stmt/stmt.h"

typedef struct ast_compound_stmt
{
    ast_stmt_t base;
    ast_stmt_t* inner_stmts; // TODO: vec_t
} ast_compound_stmt_t;

ast_stmt_t* ast_compound_stmt_create(ast_stmt_t* inner_stmt);

#endif