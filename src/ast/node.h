#ifndef AST_NODE__H
#define AST_NODE__H

static constexpr int AST_NODE_PRINT_INDENTATION_WIDTH = 2;

typedef struct ast_node ast_node_t;
typedef struct ast_visitor ast_visitor_t;

typedef struct source_location
{
    char* filename;
    int line;
    int column;
} source_location_t;

typedef struct ast_node_vtable
{
    void (*accept)(void* self_, ast_visitor_t* visitor, void* out);
    void (*destroy)(void* self_);
} ast_node_vtable_t;

struct ast_node
{
    ast_node_vtable_t* vtable;
    source_location_t source_begin;
    source_location_t source_end;
};

#define AST_NODE(ptr) ((ast_node_t*)(ptr))

void ast_node_destroy(void* node);

// begin and end ownership is transferred
void ast_node_set_source(void* node, source_location_t* begin, source_location_t* end);

// Set source location from begin to and including last child ast_node_t.
// begin ownership is transferred
void ast_node_set_source_upto(void* node, source_location_t* begin, void* last_node);

// Set source location from first child ast_node_t to end.
// begin ownership is transferred
void ast_node_set_source_from(void* node, void* first_node, source_location_t* end);

// Deconstruct data held in abstract class. Should be called by children inheriting from this class.
void ast_node_deconstruct(ast_node_t* node);

void set_source_location(source_location_t* location, const char* filename, int line, int column);

void source_location_deinit(source_location_t* location);

void source_location_move(source_location_t* dst, source_location_t* src);

#endif
