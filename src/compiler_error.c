#include "compiler_error.h"

#include <stdlib.h>
#include <string.h>

compiler_error_t* compiler_error_create(bool warning, ast_node_t* offender, const char* description,
    const char* source_file, int line, int column)
{
    compiler_error_t* error = malloc(sizeof(*error));

    *error = (compiler_error_t){
        .is_warning = warning,
        .offender = offender,
        .description = strdup(description),
        .source_file = strdup(source_file),
        .line = line,
        .column = column,
    };

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
