#ifndef COMPILE_ERROR__H
#define COMPILE_ERROR__H

#include "ast/node.h"

// Class used for any errors or warnings output by the compiler that is targetting the user.
typedef struct compiler_error
{
    bool is_warning;
    char* description;

    // If offender == nullptr, location is defined by (source_file, line, column),
    // Otherwise, location is defined by offender.
    ast_node_t* offender;
    char* source_file;
    int line;
    int column;
} compiler_error_t;

compiler_error_t* compiler_error_create_for_source(bool warning, const char* description,
    const char* source_file, int line, int column);

compiler_error_t* compiler_error_create_for_ast(bool warning, const char* description,
    ast_node_t* offender);

void compiler_error_destroy(compiler_error_t* error);

void compiler_error_destroy_void(void* error);

#endif
