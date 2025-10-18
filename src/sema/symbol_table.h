#ifndef SEMA_SYMBOL_TABLE__H
#define SEMA_SYMBOL_TABLE__H

#include "common/containers/hash_table.h"
#include "sema/symbol.h"

typedef enum scope_kind
{
    SCOPE_GLOBAL,
    SCOPE_FUNCTION,
    SCOPE_BLOCK,
    SCOPE_CLASS,
    SCOPE_METHOD,
} scope_kind_t;

typedef struct symbol_table symbol_table_t;

struct symbol_table
{
    symbol_table_t* parent;
    scope_kind_t kind;
    hash_table_t map;
};

symbol_table_t* symbol_table_create(symbol_table_t* parent, scope_kind_t kind);

void symbol_table_destroy(symbol_table_t* table);

void symbol_table_destroy_void(void* table);

void symbol_table_insert(symbol_table_t* table, symbol_t* symbol);

symbol_t* symbol_table_lookup(symbol_table_t* table, const char* name);

symbol_t* symbol_table_lookup_local(symbol_table_t* table, const char* name);

#endif
