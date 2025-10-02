#include "compound_stmt.h"
#include "ast/node.h"
#include "ast/stmt/stmt.h"

#include <stdio.h>
#include <stdlib.h>

static void ast_compound_stmt_print(ast_node_t* _self, int indentation);
static void ast_compound_stmt_destroy(ast_node_t* _self);

static ast_node_vtable_t ast_compound_stmt_vtable =
{
    .print = ast_compound_stmt_print,
    .destroy = ast_compound_stmt_destroy
};

ast_compound_stmt_t* ast_compound_stmt_create(ast_stmt_t* inner_stmt)
{
    ast_compound_stmt_t* compound_stmt = malloc(sizeof(*compound_stmt));

    AST_NODE(compound_stmt)->vtable = &ast_compound_stmt_vtable;
    compound_stmt->inner_stmts = inner_stmt;

    return compound_stmt;
}

static void ast_compound_stmt_print(ast_node_t* _self, int indentation)
{
    ast_compound_stmt_t* self = (ast_compound_stmt_t*)_self;

    printf("%*sCompoundStmt\n", indentation, "");
    ast_node_print(AST_NODE(self->inner_stmts), indentation + AST_NODE_PRINT_INDENTATION_WIDTH);
}

static void ast_compound_stmt_destroy(ast_node_t* _self)
{
    ast_compound_stmt_t* self = (ast_compound_stmt_t*)_self;

    if (self != nullptr)
    {
        ast_stmt_deconstruct((ast_stmt_t*)self);
        free(self);
    }
}
