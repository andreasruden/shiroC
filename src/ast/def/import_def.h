#ifndef AST_DEF_IMPORT__H
#define AST_DEF_IMPORT__H

#include "ast/def/def.h"

typedef struct ast_import_def
{
    ast_def_t base;
    char* project_name;
    char* module_name;
} ast_import_def_t;

ast_def_t* ast_import_def_create(const char* project_name, const char* module_name);

#endif
