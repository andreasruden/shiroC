#ifndef SEMA_SYMBOL__H
#define SEMA_SYMBOL__H

#include "ast/node.h"
#include "ast/type.h"
#include "common/containers/hash_table.h"
#include "common/containers/string.h"
#include "common/containers/vec.h"

typedef struct symbol_table symbol_table_t;

typedef enum symbol_kind
{
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    SYMBOL_PARAMETER,
    SYMBOL_CLASS,
    SYMBOL_MEMBER,
    SYMBOL_METHOD,
} symbol_kind_t;

typedef struct symbol
{
    char* name;
    symbol_kind_t kind;
    ast_node_t* ast;       // nullptr for imported symbols (memory not owned by us)
    ast_type_t* type;
    char* source_project;  // nullptr for internal local symbols
    char* source_module;   // nullptr for internal local symbols
    char* class_scope;     // nullptr for anything but SYMBOL_METHOD
    char* fully_qualified_name;

    // Kind-specific data
    union
    {
        struct
        {
            vec_t parameters;         // symbol_t*
            ast_type_t* return_type;
            size_t overload_index;
        } function;  // used by function & method

        struct
        {
            hash_table_t members;     // symbol_t*
            symbol_table_t* methods;
        } class;

        struct
        {
            ast_expr_t* default_value;  // memory owned by us
        } member;
    } data;
} symbol_t;

// FIXME: It should be easier to create a valid symbol (right now you need to fill in a bunch of stuff after)
symbol_t* symbol_create(const char* name, symbol_kind_t kind, void* ast, const char* project, const char* module,
    const char* class);

// If include_ast=true, a pointer to the AST will be included (AST is never owned by symbol)
symbol_t* symbol_clone(symbol_t* source, bool include_ast);

void symbol_destroy(symbol_t* symbol);

void symbol_destroy_void(void* symbol);

#endif
