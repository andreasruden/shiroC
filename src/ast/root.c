#include "root.h"

#include "ast/node.h"

#include <stdio.h>
#include <stdlib.h>

static void ast_root_print(ast_node_t* _self, int indentation);
static void ast_root_destroy(ast_node_t* _self);

static ast_node_vtable_t ast_root_vtable =
{
    .print = ast_root_print,
    .destroy = ast_root_destroy
};

ast_root_t* ast_root_create(ast_def_t* def)
{
    ast_root_t* root = malloc(sizeof(*root));

    root->base.vtable = &ast_root_vtable;
    root->tl_def = def;

    return root;
}

static void ast_root_print(ast_node_t* _self, int indentation)
{
    (void)indentation;
    ast_root_t* self = (ast_root_t*)_self;

    printf("Root\n");
    if (self->tl_def != nullptr)
        ast_node_print(AST_NODE(self->tl_def), AST_NODE_PRINT_INDENTATION_WIDTH);
}

static void ast_root_destroy(ast_node_t* _self)
{
    ast_root_t* self = (ast_root_t*)_self;

    if (self != nullptr)
    {
        ast_root_destroy(AST_NODE(self->tl_def));
        free(self);
    }
}
