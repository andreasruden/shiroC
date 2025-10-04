#ifndef AST_ROOT__H
#define AST_ROOT__H

#include "ast/def/def.h"
#include "ast/node.h"
#include "common/containers/ptr_vec.h"

typedef struct ast_root
{
    ast_node_t base;
    ptr_vec_t tl_defs;  // ptr_vec<ast_def_t*>
} ast_root_t;

// Note: Ownership of defs is transferred.
ast_root_t* ast_root_create(ptr_vec_t* defs);

__attribute__((sentinel))
ast_root_t* ast_root_create_va(ast_def_t* first, ...);

#endif
