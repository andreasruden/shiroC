#ifndef AST_PARAM_DECL__H
#define AST_PARAM_DECL__H

#include "ast/decl/decl.h"

typedef struct ast_param_decl
{
    ast_decl_t base;
    char* name;
    char* type;
} ast_param_decl_t;

ast_decl_t* ast_param_decl_create(const char* name, const char* type);

#endif
