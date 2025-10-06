#include "stmt.h"

void ast_stmt_deconstruct(ast_stmt_t* stmt)
{
    ast_node_deconstruct((ast_node_t*)stmt);
}
