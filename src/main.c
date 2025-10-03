#include "ast/node.h"
#include "ast/printer.h"
#include "lexer.h"
#include "parser.h"

#include <stdio.h>

int main()
{
    parser_t* parser = parser_create(lexer_create("int main() { return 0; }"));
    ast_root_t* root = parser_parse(parser);
    if (root == nullptr)
    {
        puts("Parser failed parsing");
        return 1;
    }

    ast_printer_t* printer = ast_printer_create();
    ast_printer_print_ast(printer, AST_NODE(root));

    ast_node_destroy(AST_NODE(root));
}
