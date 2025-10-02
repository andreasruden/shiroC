#ifndef AST_ROOT__H
#define AST_ROOT__H

#include "ast/node.h"
#include "ast/def/def.h"

typedef struct ast_root
{
    ast_node_t base;
    ast_def_t* tl_def; // TODO: vec_t
} ast_root_t;

ast_root_t* ast_root_create(ast_def_t* def);

#endif
