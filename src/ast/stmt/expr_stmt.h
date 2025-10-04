#ifndef AST_STMT_EXPR__H
#define AST_STMT_EXPR__H

#include "ast/expr/expr.h"
#include "stmt.h"

typedef struct ast_expr_stmt
{
    ast_stmt_t base;
    ast_expr_t* expr;
} ast_expr_stmt_t;

ast_stmt_t* ast_expr_stmt_create(ast_expr_t* expr);

#endif
