#include "root.h"

#include "ast/node.h"
#include "ast/visitor.h"

#include <stdlib.h>

static void ast_root_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_root_destroy(void* self_);

static ast_node_vtable_t ast_root_vtable =
{
    .accept = ast_root_accept,
    .destroy = ast_root_destroy
};

ast_root_t* ast_root_create(ast_def_t* def)
{
    ast_root_t* root = malloc(sizeof(*root));

    root->base.vtable = &ast_root_vtable;
    root->tl_def = def;

    return root;
}

static void ast_root_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_root_t* self = self_;
    visitor->visit_root(visitor, self, out);
}

static void ast_root_destroy(void* self_)
{
    ast_root_t* self = (ast_root_t*)self_;

    if (self != nullptr)
    {
        ast_root_destroy(AST_NODE(self->tl_def));
        free(self);
    }
}
