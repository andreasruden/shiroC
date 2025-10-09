#ifndef AST_EXPR__H
#define AST_EXPR__H

#include "ast/node.h"
#include "ast/type.h"

typedef struct ast_expr
{
    ast_node_t base;
    ast_type_t* type;   // null during parsing, filled in by semantic analysis
} ast_expr_t;

// Deconstruct data held in abstract class. Should be called by children inheriting from this class.
void ast_expr_deconstruct(ast_expr_t* expr);

#endif
