#ifndef AST_NODE__H
#define AST_NODE__H

static constexpr int AST_NODE_PRINT_INDENTATION_WIDTH = 2;

typedef struct ast_node ast_node_t;

typedef struct ast_node_vtable
{
    void (*print)(ast_node_t* self, int indentation);
    void (*destroy)(ast_node_t* self);
} ast_node_vtable_t;

struct ast_node
{
    ast_node_vtable_t* vtable;
};

#define AST_NODE(ptr) ((ast_node_t*)(ptr))

void ast_node_print(ast_node_t* node, int indentation);

void ast_node_destroy(ast_node_t* node);

#endif
