#ifndef AST_DEF_CLASS_DEF__H
#define AST_DEF_CLASS_DEF__H

#include "ast/def/def.h"
#include "common/containers/vec.h"

typedef struct ast_class_def
{
    ast_def_t base;
    vec_t members;  // vec<ast_member_decl_t*>
    vec_t methods;  // vec<ast_method_def_t*>
    bool exported;
} ast_class_def_t;

// Ownership of members and methods is transferred
ast_def_t* ast_class_def_create(const char* name, vec_t* members, vec_t* methods, bool exported);

__attribute__((sentinel))
ast_def_t* ast_class_def_create_va(const char* name, ...);

#endif
