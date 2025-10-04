#include "int_lit.h"

#include "ast/visitor.h"

#include <stdlib.h>

static void ast_int_lit_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_int_lit_destroy(void* self_);

static ast_node_vtable_t ast_int_lit_vtable =
{
    .accept = ast_int_lit_accept,
    .destroy = ast_int_lit_destroy
};

ast_expr_t* ast_int_lit_create(int value)
{
    ast_int_lit_t* int_lit = malloc(sizeof(*int_lit));

    AST_NODE(int_lit)->vtable = &ast_int_lit_vtable;
    int_lit->value = value;

    return (ast_expr_t*)int_lit;
}

static void ast_int_lit_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_int_lit_t* self = self_;
    visitor->visit_int_lit(visitor, self, out);
}

static void ast_int_lit_destroy(void* self_)
{
    ast_int_lit_t* self = self_;

    if (self != nullptr)
    {
        ast_expr_deconstruct((ast_expr_t*)self);
        free(self);
    }
}
