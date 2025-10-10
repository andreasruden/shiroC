#include "ast/node.h"
#include "ast/printer.h"
#include "parser/parser.h"

#include <stdio.h>
#include <stdlib.h>

int main()
{
    parser_t* parser = parser_create();
    parser_set_source(parser, "hardcoded", "int main() { return 0; }");
    ast_root_t* root = parser_parse(parser);
    if (root == nullptr)
    {
        puts("Parser failed parsing");
        return 1;
    }

    ast_printer_t* printer = ast_printer_create();
    char* str = ast_printer_print_ast(printer, AST_NODE(root));
    puts(str);
    free(str);

    ast_node_destroy(root);
}
