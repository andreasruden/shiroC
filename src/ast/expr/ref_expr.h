#ifndef AST_EXPR_REF__H
#define AST_EXPR_REF__H

#include "ast/expr/expr.h"

typedef struct symbol symbol_t;

typedef struct ast_ref_expr
{
    ast_expr_t base;
    char* name;
    symbol_t* resolved_symbol;  // resolved during semantic analysis
} ast_ref_expr_t;

ast_expr_t* ast_ref_expr_create(const char* name);

#endif
