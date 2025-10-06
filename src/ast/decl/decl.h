#ifndef AST_DECL__H
#define AST_DECL__H

#include "ast/node.h"

typedef struct ast_decl
{
    ast_node_t base;
} ast_decl_t;

// Deconstruct data held in abstract class. Should be called by children inheriting from this class.
static inline void ast_decl_deconstruct(ast_decl_t* decl)
{
    ast_node_deconstruct((ast_node_t*)decl);
}

#endif
