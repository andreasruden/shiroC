#include "node.h"
#include "common/containers/vec.h"
#include "compiler_error.h"

#include <stdlib.h>
#include <string.h>

void ast_node_destroy(void* node)
{
    ast_node_t* ast_node = node;
    if (node == nullptr)
        return;
    ast_node->vtable->destroy(node);
}

void ast_node_set_source(void* node, source_location_t* begin, source_location_t* end)
{
    ast_node_t* ast_node = node;
    source_location_move(&ast_node->source_begin, begin);
    source_location_move(&ast_node->source_end, end);
}

void ast_node_add_error(void* node, compiler_error_t* error)
{
    ast_node_t* ast_node = node;
    if (ast_node->errors == nullptr)
        ast_node->errors = vec_create(compiler_error_destroy_void);
    vec_push(ast_node->errors, error);
}

void ast_node_deconstruct(ast_node_t* node)
{
    source_location_deinit(&node->source_begin);
    source_location_deinit(&node->source_end);
    vec_destroy(node->errors);
}

void set_source_location(source_location_t* location, const char* filename, int line, int column)
{
    location->filename = strdup(filename);
    location->line = line;
    location->column = column;
}

void source_location_deinit(source_location_t* location)
{
    free(location->filename);
}

void source_location_move(source_location_t* dst, source_location_t* src)
{
    source_location_deinit(dst);
    *dst = *src;
    *src = (source_location_t){};
}
