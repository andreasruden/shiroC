#ifndef AST_NODE__H
#define AST_NODE__H

#include "common/containers/vec.h"
#include "compiler_error.h"

static constexpr int AST_NODE_PRINT_INDENTATION_WIDTH = 2;

// Order of categories must remain unchanged
typedef enum ast_node_kind
{
    AST_ROOT,

    // Declarations
    AST_DECL_MEMBER,
    AST_DECL_PARAM,
    AST_DECL_VAR,
    AST_DECL_END, // Sentinel

    // Definitions
    AST_DEF_CLASS,
    AST_DEF_FN,
    AST_DEF_METHOD,
    AST_DEF_END, // Sentinel

    // Expressions
    AST_EXPR_ARRAY_LIT,
    AST_EXPR_ARRAY_SLICE,
    AST_EXPR_ARRAY_SUBSCRIPT,
    AST_EXPR_BIN_OP,
    AST_EXPR_BOOL_LIT,
    AST_EXPR_CALL,
    AST_EXPR_CAST,
    AST_EXPR_COERCION,
    AST_EXPR_CONSTRUCT,
    AST_EXPR_FLOAT_LIT,
    AST_EXPR_INT_LIT,
    AST_EXPR_MEMBER_ACCESS,
    AST_EXPR_MEMBER_INIT,
    AST_EXPR_METHOD_CALL,
    AST_EXPR_NULL_LIT,
    AST_EXPR_UNINIT_LIT,
    AST_EXPR_PAREN,
    AST_EXPR_REF,
    AST_EXPR_SELF,
    AST_EXPR_STR_LIT,
    AST_EXPR_UNARY_OP,
    AST_EXPR_END, // Sentinel

    // Statements
    AST_STMT_BREAK,
    AST_STMT_COMPOUND,
    AST_STMT_CONTINUE,
    AST_STMT_DECL,
    AST_STMT_EXPR,
    AST_STMT_FOR,
    AST_STMT_IF,
    AST_STMT_INC_DEC,
    AST_STMT_RETURN,
    AST_STMT_WHILE,
    AST_STMT_END,
} ast_node_kind_t;

typedef struct ast_node ast_node_t;
typedef struct ast_visitor ast_visitor_t;
typedef struct ast_transformer ast_transformer_t;
typedef struct compiler_error compiler_error_t;

typedef struct source_location
{
    char* filename;
    int line;
    int column;
} source_location_t;

typedef struct ast_node_vtable
{
    void (*accept)(void* self_, ast_visitor_t* visitor, void* out);
    void* (*accept_transformer)(void* self_, ast_transformer_t* transformer, void* out);
    void (*destroy)(void* self_);
} ast_node_vtable_t;

struct ast_node
{
    ast_node_kind_t kind;
    ast_node_vtable_t* vtable;
    source_location_t source_begin;
    source_location_t source_end;
    vec_t* errors;  // compiler_error_t*; note that this includes warnings
};

#define AST_NODE(node) ((ast_node_t*)(node))
#define AST_KIND(node) (((ast_node_t*)(node))->kind)

#define AST_IS_DECL(node) (AST_KIND(node) > AST_ROOT && AST_KIND(node) < AST_DECL_END)
#define AST_IS_DEF(node) (AST_KIND(node) > AST_DECL_END && AST_KIND(node) < AST_DEF_END)
#define AST_IS_EXPR(node) (AST_KIND(node) > AST_DEF_END && AST_KIND(node) < AST_EXPR_END)
#define AST_IS_STMT(node) (AST_KIND(node) > AST_EXPR_END && AST_KIND(node) < AST_STMT_END)

void ast_node_destroy(void* node);

// begin and end ownership is transferred
void ast_node_set_source(void* node, source_location_t* begin, source_location_t* end);

void ast_node_add_error(void* node, compiler_error_t* error);

// Deconstruct data held in abstract class. Should be called by children inheriting from this class.
void ast_node_deconstruct(ast_node_t* node);

void set_source_location(source_location_t* location, const char* filename, int line, int column);

void source_location_deinit(source_location_t* location);

void source_location_move(source_location_t* dst, source_location_t* src);

#endif
