#include "expr.h"

void ast_expr_deconstruct(ast_expr_t* expr)
{
    ast_node_deconstruct((ast_node_t*)expr);
}
