#include "for_stmt.h"

#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdarg.h>
#include <stdlib.h>

static void ast_for_stmt_accept(void* self_, ast_visitor_t* visitor, void* out);
static void* ast_for_stmt_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_for_stmt_destroy(void* self_);

static ast_node_vtable_t ast_for_stmt_vtable =
{
    .accept = ast_for_stmt_accept,
    .accept_transformer = ast_for_stmt_accept_transformer,
    .destroy = ast_for_stmt_destroy
};

ast_stmt_t* ast_for_stmt_create(ast_stmt_t* init_stmt, ast_expr_t* cond_expr, ast_stmt_t* post_stmt,
    ast_stmt_t* body)
{
    ast_for_stmt_t* for_stmt = malloc(sizeof(*for_stmt));

    *for_stmt = (ast_for_stmt_t){
        .init_stmt = init_stmt,
        .cond_expr = cond_expr,
        .post_stmt = post_stmt,
        .body = body,
    };
    AST_NODE(for_stmt)->vtable = &ast_for_stmt_vtable;
    AST_NODE(for_stmt)->kind = AST_STMT_FOR;

    return (ast_stmt_t*)for_stmt;
}

static void ast_for_stmt_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_for_stmt_t* self = self_;
    visitor->visit_for_stmt(visitor, self, out);
}

static void* ast_for_stmt_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_for_stmt_t* self = self_;
    return transformer->transform_for_stmt(transformer, self, out);
}

static void ast_for_stmt_destroy(void* self_)
{
    ast_for_stmt_t* self = self_;
    if (self == nullptr)
        return;

    ast_stmt_deconstruct((ast_stmt_t*)self);
    ast_node_destroy(self->init_stmt);
    ast_node_destroy(self->cond_expr);
    ast_node_destroy(self->post_stmt);
    ast_node_destroy(self->body);
    free(self);
}
