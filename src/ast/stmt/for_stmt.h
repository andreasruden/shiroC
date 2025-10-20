#ifndef AST_STMT_FOR__H
#define AST_STMT_FOR__H

#include "ast/expr/expr.h"
#include "ast/stmt/stmt.h"

typedef struct ast_for_stmt
{
    ast_stmt_t base;

    // structure: for (init_stmt; cond_expr; post_stmt) body
    ast_stmt_t* init_stmt;  // nullable
    ast_expr_t* cond_expr;  // nullable
    ast_stmt_t* post_stmt;  // nullable
    ast_stmt_t* body;       // correct syntax requires this to be ast_compound_stmt, but AST can be
                            // constructed (with errors) with any statement, so don't assume type
} ast_for_stmt_t;

ast_stmt_t* ast_for_stmt_create(ast_stmt_t* init_stmt, ast_expr_t* cond_expr, ast_stmt_t* post_stmt,
    ast_stmt_t* body);

#endif
