#include "return_stmt.h"

#include "ast/node.h"
#include "ast/stmt/stmt.h"
#include "ast/visitor.h"

#include <stdlib.h>

static void ast_return_stmt_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_return_stmt_destroy(void* self_);

static ast_node_vtable_t ast_return_stmt_vtable =
{
    .accept = ast_return_stmt_accept,
    .destroy = ast_return_stmt_destroy
};

ast_stmt_t* ast_return_stmt_create(ast_expr_t* value_expr)
{
    ast_return_stmt_t* return_stmt = malloc(sizeof(*return_stmt));

    AST_NODE(return_stmt)->vtable = &ast_return_stmt_vtable;
    return_stmt->value_expr = value_expr;

    return (ast_stmt_t*)return_stmt;
}

static void ast_return_stmt_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_return_stmt_t* self = self_;
    visitor->visit_return_stmt(visitor, self, out);
}

static void ast_return_stmt_destroy(void* self_)
{
    ast_return_stmt_t* self = (ast_return_stmt_t*)self_;

    if (self != nullptr)
    {
        ast_stmt_deconstruct((ast_stmt_t*)self);
        ast_node_destroy(AST_NODE(self->value_expr));
        free(self);
    }
}
