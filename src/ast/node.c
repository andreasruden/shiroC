#include "node.h"

void ast_node_destroy(void* node)
{
    ast_node_t* ast_node = node;
    ast_node->vtable->destroy(node);
}
