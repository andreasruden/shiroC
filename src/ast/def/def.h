#ifndef AST_DEF_DEFINITION__H
#define AST_DEF_DEFINITION__H

#include "ast/node.h"

typedef struct ast_def
{
    ast_node_t base;
    char* name;
} ast_def_t;

// Deconstruct data held in abstract class. Should be called by children inheriting from this class.
void ast_def_deconstruct(ast_def_t* def);

#endif
