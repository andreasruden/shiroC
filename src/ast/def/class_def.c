#include "class_def.h"

#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static void ast_class_def_accept(void* self_, ast_visitor_t* visitor, void* out);
static void ast_class_def_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_class_def_destroy(void* self_);

static ast_node_vtable_t ast_class_def_vtable =
{
    .accept = ast_class_def_accept,
    .accept_transformer = ast_class_def_accept_transformer,
    .destroy = ast_class_def_destroy
};

ast_def_t* ast_class_def_create(const char* name, vec_t* members, vec_t* methods)
{
    ast_class_def_t* class_def = malloc(sizeof(*class_def));

    *class_def = (ast_class_def_t){
        .base.name = strdup(name),
    };
    vec_move(&class_def->members, members);
    vec_move(&class_def->methods, methods);
    AST_NODE(class_def)->vtable = &ast_class_def_vtable;
    AST_NODE(class_def)->kind = AST_DEF_CLASS;

    return (ast_def_t*)class_def;
}

ast_def_t* ast_class_def_create_va(const char* name, ...)
{
    vec_t members = VEC_INIT(ast_node_destroy);
    vec_t methods = VEC_INIT(ast_node_destroy);

    va_list args;
    va_start(args, name);
    ast_node_t* def;
    while ((def = va_arg(args, ast_node_t*)) != nullptr) {
        if (AST_KIND(def) == AST_DEF_METHOD)
            vec_push(&methods, def);
        else if (AST_KIND(def) == AST_DECL_MEMBER)
            vec_push(&members, def);
        else
            panic("Bad type %d", AST_KIND(def));
    }
    va_end(args);

    return ast_class_def_create(name, &members, &methods);
}

static void ast_class_def_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_class_def_t* self = self_;
    visitor->visit_class_def(visitor, self, out);
}

static void ast_class_def_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_class_def_t** self = self_;
    transformer->transform_class_def(transformer, self, out);
}

static void ast_class_def_destroy(void* self_)
{
    ast_class_def_t* self = self_;
    if (self == nullptr)
        return;

    ast_def_deconstruct((ast_def_t*)self);
    vec_deinit(&self->members);
    vec_deinit(&self->methods);
    free(self);
}
