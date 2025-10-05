#ifndef AST_NODE__H
#define AST_NODE__H

static constexpr int AST_NODE_PRINT_INDENTATION_WIDTH = 2;

typedef struct ast_node ast_node_t;
typedef struct ast_visitor ast_visitor_t;

typedef struct ast_node_vtable
{
    void (*accept)(void* self_, ast_visitor_t* visitor, void* out);
    void (*destroy)(void* self_);
} ast_node_vtable_t;

struct ast_node
{
    ast_node_vtable_t* vtable;
};

#define AST_NODE(ptr) ((ast_node_t*)(ptr))

void ast_node_destroy(void* node);

#endif
