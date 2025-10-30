#ifndef AST_TYPE_PARAM_DECL__H
#define AST_TYPE_PARAM_DECL__H

#include "ast/decl/decl.h"

typedef struct symbol symbol_t;

typedef struct ast_type_param_decl
{
    ast_decl_t base;
    char* name;
    symbol_t* symbol;  // set by decl_collector & used in semantic analysis
} ast_type_param_decl_t;

ast_decl_t* ast_type_param_decl_create(const char* name);

#endif
