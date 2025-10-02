#include "def.h"

#include <stdlib.h>

void ast_def_deconstruct(ast_def_t* def)
{
    if (def != nullptr)
        free(def->name);
}
