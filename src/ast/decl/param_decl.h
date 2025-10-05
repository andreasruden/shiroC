#ifndef AST_PARAM_DECL__H
#define AST_PARAM_DECL__H

#include "ast/decl/decl.h"

typedef struct ast_param_decl
{
    ast_decl_t base;
    char* type;
    char* name;
} ast_param_decl_t;

ast_decl_t* ast_param_decl_create(const char* type, const char* name);

#endif
