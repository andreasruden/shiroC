#include "fn_def.h"

#include "ast/node.h"
#include "ast/visitor.h"

#include <stdlib.h>
#include <string.h>

static void ast_fn_def_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_fn_def_destroy(void* self_);

static ast_node_vtable_t ast_fn_def_vtable =
{
    .accept = ast_fn_def_accept,
    .destroy = ast_fn_def_destroy
};

ast_fn_def_t* ast_fn_def_create(const char* name, ast_compound_stmt_t* body)
{
    ast_fn_def_t* fn_def = malloc(sizeof(*fn_def));

    AST_NODE(fn_def)->vtable = &ast_fn_def_vtable;
    fn_def->base.name = strdup(name);
    fn_def->body = body;

    return fn_def;
}

static void ast_fn_def_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_fn_def_t* self = self_;
    visitor->visit_fn_def(visitor, self, out);
}

static void ast_fn_def_destroy(void* self_)
{
    ast_fn_def_t* self = self_;

    if (self != nullptr)
    {
        ast_def_deconstruct((ast_def_t*)self);
        ast_node_destroy(AST_NODE(self->body));
        free(self);
    }
}
