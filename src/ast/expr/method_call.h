#ifndef AST_EXPR_MEMBER_CALL__H
#define AST_EXPR_MEMBER_CALL__H

#include "ast/expr/expr.h"
#include "common/containers/vec.h"

typedef struct ast_method_call
{
    ast_expr_t base;
    ast_expr_t* instance;
    char* method_name;
    vec_t arguments;        // ast_expr_t*
    size_t overload_index;  // set by SEMA during overload resolution
} ast_method_call_t;

ast_expr_t* ast_method_call_create(ast_expr_t* instance, const char* method_name, vec_t* arguments);

__attribute__((sentinel))
ast_expr_t* ast_method_call_create_va(ast_expr_t* instance, const char* method_name, ...);

#endif
