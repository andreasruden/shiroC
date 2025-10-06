#include "def.h"
#include "ast/node.h"

#include <stdlib.h>

void ast_def_deconstruct(ast_def_t* def)
{
    ast_node_deconstruct((ast_node_t*)def);
    if (def != nullptr)
        free(def->name);
}
