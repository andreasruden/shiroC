#include "ref_expr.h"

#include "ast/visitor.h"

#include <stdlib.h>
#include <string.h>

static void ast_ref_expr_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_ref_expr_destroy(void* self_);

static ast_node_vtable_t ast_ref_expr_vtable =
{
    .accept = ast_ref_expr_accept,
    .destroy = ast_ref_expr_destroy
};

ast_expr_t* ast_ref_expr_create(const char* name)
{
    ast_ref_expr_t* ref_expr = calloc(1, sizeof(*ref_expr));

    *ref_expr = (ast_ref_expr_t){
        .base = AST_EXPR_INIT,
        .name = strdup(name)
    };
    AST_NODE(ref_expr)->vtable = &ast_ref_expr_vtable;
    AST_NODE(ref_expr)->kind = AST_EXPR_REF;

    return (ast_expr_t*)ref_expr;
}

static void ast_ref_expr_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_ref_expr_t* self = self_;
    visitor->visit_ref_expr(visitor, self, out);
}

static void ast_ref_expr_destroy(void* self_)
{
    ast_ref_expr_t* self = self_;
    if (self == nullptr)
        return;

    ast_expr_deconstruct((ast_expr_t*)self);
    free(self->name);
    free(self);
}
