#ifndef COMPILE_ERROR__H
#define COMPILE_ERROR__H

#include "ast/node.h"

// Class used for any errors or warnings output by the compiler that is targetting the user.
typedef struct compiler_error
{
    bool is_warning;
    ast_node_t* offender;  // can be nullptr, can also become invalid if AST that caused error changes
    char* description;
    char* source_file;
    int line;
    int column;
} compiler_error_t;

compiler_error_t* compiler_error_create(bool warning, ast_node_t* offender, const char* description,
    const char* source_file, int line, int column);

void compiler_error_destroy(compiler_error_t* error);

void compiler_error_destroy_void(void* error);

#endif
