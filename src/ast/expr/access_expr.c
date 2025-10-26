#include "access_expr.h"

#include "ast/expr/expr.h"
#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdlib.h>

static void ast_access_expr_accept(void* self_, ast_visitor_t* visitor, void* out);
static void* ast_access_expr_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_access_expr_destroy(void* self_);

static ast_node_vtable_t ast_access_expr_vtable =
{
    .accept = ast_access_expr_accept,
    .accept_transformer = ast_access_expr_accept_transformer,
    .destroy = ast_access_expr_destroy
};

ast_expr_t* ast_access_expr_create(ast_expr_t* outer, ast_expr_t* inner)
{
    ast_access_expr_t* access_expr = malloc(sizeof(*access_expr));

    *access_expr = (ast_access_expr_t){
        .base = AST_EXPR_INIT,
        .outer = outer,
        .inner = inner,
    };
    AST_NODE(access_expr)->vtable = &ast_access_expr_vtable;
    AST_NODE(access_expr)->kind = AST_EXPR_ACCESS;

    return (ast_expr_t*)access_expr;
}

static void ast_access_expr_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_access_expr_t* self = self_;
    visitor->visit_access_expr(visitor, self, out);
}

static void* ast_access_expr_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_access_expr_t* self = self_;
    return transformer->transform_access_expr(transformer, self, out);
}

static void ast_access_expr_destroy(void* self_)
{
    ast_access_expr_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    if (self->outer != nullptr)
        ast_node_destroy(self->outer);
    if (self->inner != nullptr)
        ast_node_destroy(self->inner);
    free(self);
}
