#include "import_def.h"

#include "ast/node.h"
#include "ast/transformer.h"
#include "ast/visitor.h"

#include <stdlib.h>
#include <string.h>

static void ast_import_def_accept(void* self_, ast_visitor_t* visitor, void* out);
static void* ast_import_def_accept_transformer(void* self_, ast_transformer_t* transformer, void* out);
static void ast_import_def_destroy(void* self_);

static ast_node_vtable_t ast_import_def_vtable =
{
    .accept = ast_import_def_accept,
    .accept_transformer = ast_import_def_accept_transformer,
    .destroy = ast_import_def_destroy
};

ast_def_t* ast_import_def_create(const char* project_name, const char* module_name)
{
    ast_import_def_t* import_def = malloc(sizeof(*import_def));

    *import_def = (ast_import_def_t){
        .base.name = nullptr,
        .project_name = strdup(project_name),
        .module_name = strdup(module_name),
    };
    AST_NODE(import_def)->vtable = &ast_import_def_vtable;
    AST_NODE(import_def)->kind = AST_DEF_USE;

    return (ast_def_t*)import_def;
}

static void ast_import_def_accept(void* self_, ast_visitor_t* visitor, void* out)
{
    ast_import_def_t* self = self_;
    visitor->visit_import_def(visitor, self, out);
}

static void* ast_import_def_accept_transformer(void* self_, ast_transformer_t* transformer, void* out)
{
    ast_import_def_t* self = self_;
    return transformer->transform_import_def(transformer, self, out);
}

static void ast_import_def_destroy(void* self_)
{
    ast_import_def_t* self = self_;
    if (self == nullptr)
        return;

    ast_def_deconstruct((ast_def_t*)self);
    free(self->project_name);
    free(self->module_name);
    free(self);
}
