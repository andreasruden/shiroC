#ifndef AST_EXPR_MEMBER_ACCESS__H
#define AST_EXPR_MEMBER_ACCESS__H

#include "ast/expr/expr.h"

typedef struct ast_member_access
{
    ast_expr_t base;
    ast_expr_t* instance;
    char* member_name;
} ast_member_access_t;

ast_expr_t* ast_member_access_create(ast_expr_t* instance, const char* member_name);

#endif
