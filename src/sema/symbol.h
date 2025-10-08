#ifndef SEMA_SYMBOL__H
#define SEMA_SYMBOL__H

#include "ast/node.h"
#include "ast/type.h"
#include "common/containers/vec.h"

typedef enum symbol_kind
{
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    SYMBOL_PARAMETER,
} symbol_kind_t;

typedef struct symbol
{
    char* name;
    symbol_kind_t kind;
    ast_node_t* ast;
    char* type;

    // Kind-specific data
    union
    {
        struct
        {
            vec_t parameters;         // ast_param_decl_t*
            ast_type_t* return_type;
        } function;
    } data;
} symbol_t;

symbol_t* symbol_create(const char* name, symbol_kind_t kind, void* ast);

void symbol_destroy(symbol_t* symbol);

void symbol_destroy_void(void* symbol);

#endif
