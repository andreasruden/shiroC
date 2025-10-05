#ifndef AST_BIN_OP__H
#define AST_BIN_OP__H

#include "ast/expr/expr.h"
#include "lexer.h"

typedef struct ast_bin_op {
    ast_expr_t base;
    token_type_t op;
    ast_expr_t* lhs;
    ast_expr_t* rhs;
} ast_bin_op_t;

ast_expr_t* ast_bin_op_create(token_type_t op, ast_expr_t* lhs, ast_expr_t* rhs);

#endif
