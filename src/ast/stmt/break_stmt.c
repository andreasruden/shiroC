#include "break_stmt.h"

#include "ast/node.h"
#include "ast/stmt/stmt.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdlib.h>

static void ast_break_stmt_accept(void* self_, ast_visitor_t* visitor, void* out);
static void* ast_break_stmt_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_break_stmt_destroy(void* self_);

static ast_node_vtable_t ast_break_stmt_vtable =
{
    .accept = ast_break_stmt_accept,
    .accept_transformer = ast_break_stmt_accept_transformer,
    .destroy = ast_break_stmt_destroy
};

ast_stmt_t* ast_break_stmt_create(void)
{
    ast_break_stmt_t* break_stmt = malloc(sizeof(*break_stmt));

    *break_stmt = (ast_break_stmt_t){};
    AST_NODE(break_stmt)->vtable = &ast_break_stmt_vtable;
    AST_NODE(break_stmt)->kind = AST_STMT_BREAK;

    return (ast_stmt_t*)break_stmt;
}

static void ast_break_stmt_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_break_stmt_t* self = self_;
    visitor->visit_break_stmt(visitor, self, out);
}

static void* ast_break_stmt_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_break_stmt_t* self = self_;
    return transformer->transform_break_stmt(transformer, self, out);
}

static void ast_break_stmt_destroy(void* self_)
{
    ast_break_stmt_t* self = (ast_break_stmt_t*)self_;

    if (self == nullptr)
        return;

    ast_stmt_deconstruct((ast_stmt_t*)self);
    free(self);
}
