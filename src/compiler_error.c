#include "compiler_error.h"

#include "ast/node.h"

#include <stdlib.h>
#include <string.h>

static compiler_error_t* compiler_error_create(bool warning, const char* description, ast_node_t* offender,
    const char* source_file, int line, int column)
{
    compiler_error_t* error = malloc(sizeof(*error));

    *error = (compiler_error_t){
        .is_warning = warning,
        .offender = offender,
        .description = strdup(description),
        .source_file = source_file ? strdup(source_file) : nullptr,
        .line = line,
        .column = column,
    };

    return error;
}

compiler_error_t* compiler_error_create_for_source(bool warning, const char* description,
    const char* source_file, int line, int column)
{
    return compiler_error_create(warning, description, nullptr, source_file, line, column);
}

compiler_error_t* compiler_error_create_for_ast(bool warning, const char* description,
    ast_node_t* offender)
{
    compiler_error_t* error = compiler_error_create(warning, description, offender, nullptr, 0, 0);
    ast_node_add_error(offender, error);

    return error;
}

void compiler_error_destroy(compiler_error_t* error)
{
    if (error == nullptr)
        return;

    free(error->description);
    free(error->source_file);
    free(error);
}

void compiler_error_destroy_void(void* error)
{
    compiler_error_t* compiler_error = error;
    compiler_error_destroy(compiler_error);
}
