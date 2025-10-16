#ifndef PARSER_SHARED_H
#define PARSER_SHARED_H

#include "ast/util/printer.h"
#include "test_runner.h"

#define ASSERT_TREES_EQUAL(a, b) \
    { \
        ast_printer_t* printer = ast_printer_create(); \
        char* printed_expected_tree = ast_printer_print_ast(printer, AST_NODE(a)); \
        char* printed_root = ast_printer_print_ast(printer, AST_NODE(b)); \
        ASSERT_EQ(printed_expected_tree, printed_root); \
        free(printed_root); \
        free(printed_expected_tree); \
        ast_printer_destroy(printer); \
    }

#endif
