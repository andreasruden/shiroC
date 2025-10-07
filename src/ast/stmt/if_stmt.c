#include "if_stmt.h"

#include "ast/node.h"
#include "ast/visitor.h"

#include <stdarg.h>
#include <stdlib.h>

static void ast_if_stmt_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_if_stmt_destroy(void* self_);

static ast_node_vtable_t ast_if_stmt_vtable =
{
    .accept = ast_if_stmt_accept,
    .destroy = ast_if_stmt_destroy
};

ast_stmt_t* ast_if_stmt_create(ast_expr_t* condition, ast_stmt_t* then_branch, ast_stmt_t* else_branch)
{
    ast_if_stmt_t* if_stmt = malloc(sizeof(*if_stmt));

    *if_stmt = (ast_if_stmt_t){
        .condition = condition,
        .then_branch = then_branch,
        .else_branch = else_branch
    };
    AST_NODE(if_stmt)->vtable = &ast_if_stmt_vtable;

    return (ast_stmt_t*)if_stmt;
}

static void ast_if_stmt_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_if_stmt_t* self = self_;
    visitor->visit_if_stmt(visitor, self, out);
}

static void ast_if_stmt_destroy(void* self_)
{
    ast_if_stmt_t* self = self_;
    if (self == nullptr)
        return;

    ast_stmt_deconstruct((ast_stmt_t*)self);
    ast_node_destroy(self->condition);
    ast_node_destroy(self->then_branch);
    ast_node_destroy(self->else_branch);
    free(self);
}
