#include "bool_lit.h"

#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdlib.h>

static void ast_bool_lit_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_bool_lit_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_bool_lit_destroy(void* self_);

static ast_node_vtable_t ast_bool_lit_vtable =
{
    .accept = ast_bool_lit_accept,
    .accept_transformer = ast_bool_lit_accept_transformer,
    .destroy = ast_bool_lit_destroy
};

ast_expr_t* ast_bool_lit_create(bool value)
{
    ast_bool_lit_t* bool_lit = malloc(sizeof(*bool_lit));

    *bool_lit = (ast_bool_lit_t){
        .base = AST_EXPR_INIT,
        .value = value,
    };
    AST_NODE(bool_lit)->vtable = &ast_bool_lit_vtable;
    AST_NODE(bool_lit)->kind = AST_EXPR_BOOL_LIT;

    return (ast_expr_t*)bool_lit;
}

static void ast_bool_lit_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_bool_lit_t* self = self_;
    visitor->visit_bool_lit(visitor, self, out);
}

static void ast_bool_lit_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_bool_lit_t** self = self_;
    transformer->transform_bool_lit(transformer, self, out);
}

static void ast_bool_lit_destroy(void* self_)
{
    ast_bool_lit_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    free(self);
}
