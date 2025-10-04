#ifndef AST_STMT_COMPOUND_STMT__H
#define AST_STMT_COMPOUND_STMT__H

#include "ast/stmt/stmt.h"
#include "common/containers/ptr_vec.h"

typedef struct ast_compound_stmt
{
    ast_stmt_t base;
    ptr_vec_t inner_stmts;  // ptr_vec<ast_stmt_t*>
} ast_compound_stmt_t;

// Note: Ownership of inner_stmts is transferred.
ast_stmt_t* ast_compound_stmt_create(ptr_vec_t* inner_stmts);

__attribute__((sentinel))
ast_stmt_t* ast_compound_stmt_create_va(ast_stmt_t* first, ...);

#endif