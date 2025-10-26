#ifndef AST_EXPR_ACCESS_EXPR__H
#define AST_EXPR_ACCESS_EXPR__H

#include "ast/expr/expr.h"

// Parser cannot distinguish between member access, method call and qualified names; this node
// exists to be output by the parser and transformed during semantic analysis. It will not appear
// in an AST that has been transformed by SEMA when there were no errors.
typedef struct ast_access_expr
{
    ast_expr_t base;
    ast_expr_t* outer;
    ast_expr_t* inner;
} ast_access_expr_t;

ast_expr_t* ast_access_expr_create(ast_expr_t* outer, ast_expr_t* inner);

#endif
