#ifndef SEMA_SYMBOL__H
#define SEMA_SYMBOL__H

#include "ast/node.h"
#include "ast/type.h"
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
    SYMBOL_TRAIT_IMPL,
    SYMBOL_NAMESPACE,
    SYMBOL_TYPE_PARAMETER,
    SYMBOL_TEMPLATE_CLASS,
    SYMBOL_TEMPLATE_FUNCTION,
    SYMBOL_CLASS_INSTANCE,
    SYMBOL_FUNCTION_INSTANCE,
} symbol_kind_t;

typedef struct symbol
{
    char* name;
    symbol_kind_t kind;
    ast_node_t* ast;       // nullptr for imported symbols (memory not owned by us)
    ast_type_t* type;
    symbol_t* parent_namespace;  // memory not owned by us, nullptr for internal non member/methods
    char* fully_qualified_name;

    // Kind-specific data
    union
    {
        struct
        {
            vec_t parameters;         // symbol_t*
            ast_type_t* return_type;
            size_t overload_index;
            char* extern_abi;         // nullpt if function is not extern decl
            bool is_builtin;
        } function;  // used by function & method

        struct
        {
            symbol_table_t* symbols;  // unified members & methods (distinguished by symbol->kind)
        } class;

        struct
        {
            ast_expr_t* default_value;  // memory owned by us
        } member;

        struct
        {
            symbol_table_t* exports;  // memory owned by us
        } namespace;

        struct
        {
            vec_t type_parameters;     // vec<symbol_t*> - type parameter symbols
            vec_t instantiations;      // vec<symbol_t*> - cache of instantiated symbols
            ast_node_t* template_ast;  // original template AST (not owned by symbol)
            symbol_table_t* scope;     // scope containing type parameters (memory owned by us)
        } template;

        struct
        {
            symbol_t* template_symbol;    // pointer to template symbol (not owned)
            vec_t type_arguments;         // vec<ast_type_t*> - concrete types
            ast_node_t* instantiated_ast; // cloned and specialized AST (not owned - owned by AST tree)
        } template_instance;
    } data;
} symbol_t;

// FIXME: It should be easier to create a valid symbol (right now you need to fill in a bunch of stuff after)
// parent_namespace is needed to construct the fully-qualified-name (can be SYMBOL_NAMESPACE, SYMBOL_CLASS or nullptr)
symbol_t* symbol_create(const char* name, symbol_kind_t kind, void* ast, symbol_t* parent_namespace);

// If include_ast=true, a pointer to the AST will be included (AST is never owned by symbol)
// If parent_namespace is not nullptr, this parent namespace will be used for all cloned symbols,
// if nullptr the cloned symbol will be created without a parent namespace regardless of the source's namespace.
symbol_t* symbol_clone(symbol_t* source, bool include_ast, symbol_t* parent_namespace);

void symbol_destroy(symbol_t* symbol);

void symbol_destroy_void(void* symbol);

#endif
