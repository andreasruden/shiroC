#ifndef AST_STMT__H
#define AST_STMT__H

#include "ast/node.h"

typedef struct ast_stmt
{
    ast_node_t base;
} ast_stmt_t;

// Deconstruct data held in abstract class. Should be called by children inheriting from this class.
void ast_stmt_deconstruct(ast_stmt_t* expr);

#endif
