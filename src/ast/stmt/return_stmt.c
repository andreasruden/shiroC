#include "return_stmt.h"
#include "ast/node.h"
#include "ast/stmt/stmt.h"

#include <stdio.h>
#include <stdlib.h>

static void ast_return_stmt_print(ast_node_t* _self, int indentation);
static void ast_return_stmt_destroy(ast_node_t* _self);

static ast_node_vtable_t ast_return_stmt_vtable =
{
    .print = ast_return_stmt_print,
    .destroy = ast_return_stmt_destroy
};

ast_return_stmt_t* ast_return_stmt_create(ast_expr_t* value_expr)
{
    ast_return_stmt_t* return_stmt = malloc(sizeof(*return_stmt));

    AST_NODE(return_stmt)->vtable = &ast_return_stmt_vtable;
    return_stmt->value_expr = value_expr;

    return return_stmt;
}

static void ast_return_stmt_print(ast_node_t* _self, int indentation)
{
    ast_return_stmt_t* self = (ast_return_stmt_t*)_self;

    printf("%*sReturnStmt\n", indentation, "");
    ast_node_print(AST_NODE(self->value_expr), indentation + AST_NODE_PRINT_INDENTATION_WIDTH);
}

static void ast_return_stmt_destroy(ast_node_t* _self)
{
    ast_return_stmt_t* self = (ast_return_stmt_t*)_self;

    if (self != nullptr)
    {
        ast_stmt_deconstruct((ast_stmt_t*)self);
        ast_node_destroy(AST_NODE(self->value_expr));
        free(self);
    }
}
