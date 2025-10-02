#include "node.h"

void ast_node_print(ast_node_t* node, int indentation)
{
    node->vtable->print(node, indentation);
}

void ast_node_destroy(ast_node_t* node)
{
    node->vtable->destroy(node);
}
