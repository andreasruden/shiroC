#include "array_subscript.h"

#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdlib.h>

static void ast_array_subscript_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_array_subscript_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_array_subscript_destroy(void* self_);

static ast_node_vtable_t ast_array_subscript_vtable =
{
    .accept = ast_array_subscript_accept,
    .accept_transformer = ast_array_subscript_accept_transformer,
    .destroy = ast_array_subscript_destroy
};

ast_expr_t* ast_array_subscript_create(ast_expr_t* array, ast_expr_t* index)
{
    ast_array_subscript_t* array_subscript = malloc(sizeof(*array_subscript));

    *array_subscript = (ast_array_subscript_t){
        .base = AST_EXPR_INIT,
        .array = array,
        .index = index,
    };
    AST_NODE(array_subscript)->vtable = &ast_array_subscript_vtable;
    AST_NODE(array_subscript)->kind = AST_EXPR_ARRAY_SUBSCRIPT;

    return (ast_expr_t*)array_subscript;
}

static void ast_array_subscript_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_array_subscript_t* self = self_;
    visitor->visit_array_subscript(visitor, self, out);
}

static void ast_array_subscript_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_array_subscript_t** self = self_;
    transformer->transform_array_subscript(transformer, self, out);
}

static void ast_array_subscript_destroy(void* self_)
{
    ast_array_subscript_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    ast_node_destroy(self->array);
    ast_node_destroy(self->index);
    free(self);
}
