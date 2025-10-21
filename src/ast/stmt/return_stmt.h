#ifndef AST_STMT_RETURN_STMT__H
#define AST_STMT_RETURN_STMT__H

#include "ast/expr/expr.h"
#include "stmt.h"

typedef struct ast_return_stmt
{
    ast_stmt_t base;
    ast_expr_t* value_expr;  // can be nullptr
} ast_return_stmt_t;

ast_stmt_t* ast_return_stmt_create(ast_expr_t* value_expr);

#endif
