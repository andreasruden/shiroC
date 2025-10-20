#include "continue_stmt.h"

#include "ast/node.h"
#include "ast/stmt/stmt.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdlib.h>

static void ast_continue_stmt_accept(void* self_, ast_visitor_t* visitor, void* out);
static void* ast_continue_stmt_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_continue_stmt_destroy(void* self_);

static ast_node_vtable_t ast_continue_stmt_vtable =
{
    .accept = ast_continue_stmt_accept,
    .accept_transformer = ast_continue_stmt_accept_transformer,
    .destroy = ast_continue_stmt_destroy
};

ast_stmt_t* ast_continue_stmt_create(void)
{
    ast_continue_stmt_t* continue_stmt = malloc(sizeof(*continue_stmt));

    *continue_stmt = (ast_continue_stmt_t){};
    AST_NODE(continue_stmt)->vtable = &ast_continue_stmt_vtable;
    AST_NODE(continue_stmt)->kind = AST_STMT_CONTINUE;

    return (ast_stmt_t*)continue_stmt;
}

static void ast_continue_stmt_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_continue_stmt_t* self = self_;
    visitor->visit_continue_stmt(visitor, self, out);
}

static void* ast_continue_stmt_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_continue_stmt_t* self = self_;
    return transformer->transform_continue_stmt(transformer, self, out);
}

static void ast_continue_stmt_destroy(void* self_)
{
    ast_continue_stmt_t* self = (ast_continue_stmt_t*)self_;

    if (self == nullptr)
        return;

    ast_stmt_deconstruct((ast_stmt_t*)self);
    free(self);
}
