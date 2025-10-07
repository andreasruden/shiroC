#ifndef AST_STMT_WHILE__H
#define AST_STMT_WHILE__H

#include "ast/expr/expr.h"
#include "ast/stmt/stmt.h"

typedef struct ast_while_stmt
{
    ast_stmt_t base;
    ast_expr_t* condition;
    ast_stmt_t* body;   // correct syntax requires this to be ast_compound_stmt, but AST can be
                        // constructed (with errors) with any statement, so don't assume type
} ast_while_stmt_t;

ast_stmt_t* ast_while_stmt_create(ast_expr_t* condition, ast_stmt_t* body);

#endif
