#ifndef SEMA_SYMBOL__H
#define SEMA_SYMBOL__H

#include "ast/node.h"
#include "ast/type.h"
#include "common/containers/hash_table.h"
#include "common/containers/vec.h"

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
    ast_node_t* ast;
    ast_type_t* type;

    // Kind-specific data
    union
    {
        struct
        {
            vec_t parameters;         // ast_param_decl_t*
        } function;

        struct
        {
            hash_table_t members;     // ast_member_decl_t*
            hash_table_t methods;     // ast_method_def_t*
        } class;
    } data;
} symbol_t;

symbol_t* symbol_create(const char* name, symbol_kind_t kind, void* ast);

void symbol_destroy(symbol_t* symbol);

void symbol_destroy_void(void* symbol);

#endif
