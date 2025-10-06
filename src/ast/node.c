#include "node.h"

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

void ast_node_set_source_upto(void* node, source_location_t* begin, void* last_node)
{
    ast_node_t* ast_node = node;
    ast_node_t* last_ast_node = last_node;
    source_location_move(&ast_node->source_begin, begin);
    set_source_location(&ast_node->source_end, last_ast_node->source_end.filename, last_ast_node->source_end.line,
        last_ast_node->source_end.column);
}

void ast_node_set_source_from(void* node, void* begin_node, source_location_t* end)
{
    ast_node_t* ast_node = node;
    ast_node_t* begin_ast_node = begin_node;
    set_source_location(&ast_node->source_begin, begin_ast_node->source_begin.filename,
        begin_ast_node->source_begin.line, begin_ast_node->source_begin.column);
    source_location_move(&ast_node->source_end, end);
}

void ast_node_deconstruct(ast_node_t* node)
{
    source_location_deinit(&node->source_begin);
    source_location_deinit(&node->source_end);
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
