#include "array_slice.h"

#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdlib.h>

static void ast_array_slice_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_array_slice_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_array_slice_destroy(void* self_);

static ast_node_vtable_t ast_array_slice_vtable =
{
    .accept = ast_array_slice_accept,
    .accept_transformer = ast_array_slice_accept_transformer,
    .destroy = ast_array_slice_destroy
};

ast_expr_t* ast_array_slice_create(ast_expr_t* array, ast_expr_t* start, ast_expr_t* end)
{
    ast_array_slice_t* array_slice = malloc(sizeof(*array_slice));

    *array_slice = (ast_array_slice_t){
        .base = AST_EXPR_INIT,
        .array = array,
        .start = start,
        .end = end,
    };
    AST_NODE(array_slice)->vtable = &ast_array_slice_vtable;
    AST_NODE(array_slice)->kind = AST_EXPR_ARRAY_SLICE;

    return (ast_expr_t*)array_slice;
}

static void ast_array_slice_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_array_slice_t* self = self_;
    visitor->visit_array_slice(visitor, self, out);
}

static void ast_array_slice_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_array_slice_t** self = self_;
    transformer->transform_array_slice(transformer, self, out);
}

static void ast_array_slice_destroy(void* self_)
{
    ast_array_slice_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    ast_node_destroy(self->array);
    ast_node_destroy(self->start);
    ast_node_destroy(self->end);
    free(self);
}
