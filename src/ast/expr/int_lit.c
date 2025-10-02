#include "int_lit.h"

#include <stdio.h>
#include <stdlib.h>

static void ast_int_lit_print(ast_node_t* _self, int indentation);
static void ast_int_lit_destroy(ast_node_t* _self);

static ast_node_vtable_t ast_int_lit_vtable =
{
    .print = ast_int_lit_print,
    .destroy = ast_int_lit_destroy
};

ast_int_lit_t* ast_int_lit_create(int value)
{
    ast_int_lit_t* int_lit = malloc(sizeof(*int_lit));

    AST_NODE(int_lit)->vtable = &ast_int_lit_vtable;
    int_lit->value = value;

    return int_lit;
}

static void ast_int_lit_print(ast_node_t* _self, int indentation)
{
    ast_int_lit_t* self = (ast_int_lit_t*)_self;

    printf("%*sIntLit (value=%d)\n", indentation, "", self->value);
}

static void ast_int_lit_destroy(ast_node_t* _self)
{
    ast_int_lit_t* self = (ast_int_lit_t*)_self;

    if (self != nullptr)
    {
        ast_expr_deconstruct((ast_expr_t*)self);
        free(self);
    }
}
