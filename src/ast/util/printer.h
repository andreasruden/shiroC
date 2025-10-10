#ifndef AST_PRINTER__H
#define AST_PRINTER__H

#include "ast/node.h"

typedef struct ast_printer ast_printer_t;

ast_printer_t* ast_printer_create();

void ast_printer_destroy(ast_printer_t* printer);

char* ast_printer_print_ast(ast_printer_t* printer, ast_node_t* node);

void ast_printer_set_show_source_loc(ast_printer_t* printer, bool show);

#endif
