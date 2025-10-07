#ifndef AST_STMT_IF__H
#define AST_STMT_IF__H

#include "ast/expr/expr.h"
#include "ast/stmt/stmt.h"

typedef struct ast_if_stmt
{
    ast_stmt_t base;
    ast_expr_t* condition;
    ast_stmt_t* then_branch;    // correct syntax requires this to be ast_compound_stmt, but AST can be
    ast_stmt_t* else_branch;    // constructed (with errors) with any statement, so don't assume type
} ast_if_stmt_t;

ast_stmt_t* ast_if_stmt_create(ast_expr_t* condition, ast_stmt_t* then_branch, ast_stmt_t* else_branch);

#endif
