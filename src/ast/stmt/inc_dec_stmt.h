#ifndef AST_STMT_INC_DEC_STMT__H
#define AST_STMT_INC_DEC_STMT__H

#include "ast/expr/expr.h"
#include "stmt.h"

typedef struct ast_inc_dec_stmt
{
    ast_stmt_t base;
    ast_expr_t* operand;
    bool increment; // true for ++, false for --
} ast_inc_dec_stmt_t;

ast_stmt_t* ast_inc_dec_stmt_create(ast_expr_t* operand, bool increment);

#endif
