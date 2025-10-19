#ifndef AST_UTIL_CLONER__H
#define AST_UTIL_CLONER__H

#include "ast/expr/expr.h"

// Deep clone an expression node (returns ownership)
ast_expr_t* ast_expr_clone(ast_expr_t* expr);

#endif
