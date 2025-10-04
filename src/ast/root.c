#include "root.h"

#include "ast/node.h"
#include "ast/visitor.h"
#include "common/containers/ptr_vec.h"

#include <stdlib.h>

static void ast_root_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_root_destroy(void* self_);

static ast_node_vtable_t ast_root_vtable =
{
    .accept = ast_root_accept,
    .destroy = ast_root_destroy
};

ast_root_t* ast_root_create(ptr_vec_t* defs)
{
    ast_root_t* root = malloc(sizeof(*root));

    root->base.vtable = &ast_root_vtable;
    ptr_vec_move(&root->tl_defs, defs);

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

    if (self == nullptr)
        return;

    for (size_t i = 0; i < ptr_vec_size(&self->tl_defs); ++i)
        ast_node_destroy(AST_NODE(ptr_vec_get(&self->tl_defs, i)));
    free(self);
}
