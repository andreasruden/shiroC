#include "root.h"

#include "ast/node.h"
#include "ast/visitor.h"
#include "common/containers/ptr_vec.h"

#include <stdarg.h>
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

__attribute__((sentinel))
ast_root_t* ast_root_create_va(ast_def_t* first, ...)
{
    ptr_vec_t body = PTR_VEC_INIT;
    ptr_vec_append(&body, first);

    va_list args;
    va_start(args, first);
    ast_def_t* def;
    while ((def = va_arg(args, ast_def_t*)) != nullptr) {
        ptr_vec_append(&body, def);
    }
    va_end(args);

    return ast_root_create(&body);
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
