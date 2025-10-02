#include "fn_def.h"
#include "ast/node.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ast_fn_def_print(ast_node_t* _self, int indentation);
static void ast_fn_def_destroy(ast_node_t* _self);

static ast_node_vtable_t ast_fn_def_vtable =
{
    .print = ast_fn_def_print,
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

static void ast_fn_def_print(ast_node_t* _self, int indentation)
{
    ast_fn_def_t* self = (ast_fn_def_t*)_self;

    printf("%*sFnDef (name=%s)\n", indentation, "", self->base.name);
    ast_node_print(AST_NODE(self->body), indentation + AST_NODE_PRINT_INDENTATION_WIDTH);
}

static void ast_fn_def_destroy(ast_node_t* _self)
{
    ast_fn_def_t* self = (ast_fn_def_t*)_self;

    if (self != nullptr)
    {
        ast_def_deconstruct((ast_def_t*)self);
        ast_node_destroy(AST_NODE(self->body));
        free(self);
    }
}
