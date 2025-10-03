#include "node.h"

void ast_node_destroy(ast_node_t* node)
{
    node->vtable->destroy(node);
}
