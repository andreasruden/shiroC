#ifndef SEMA_SYMBOL_TABLE__H
#define SEMA_SYMBOL_TABLE__H

#include "common/containers/hash_table.h"
#include "sema/symbol.h"

typedef enum scope_kind
{
    SCOPE_EXPORT,
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
    hash_table_t map;  // symbol name (char*) -> vec_t<symbol_t*>*
};

symbol_table_t* symbol_table_create(symbol_table_t* parent, scope_kind_t kind);

void symbol_table_destroy(symbol_table_t* table);

void symbol_table_destroy_void(void* table);

void symbol_table_insert(symbol_table_t* table, symbol_t* symbol);

// find the first symbol matching the name in self or parent table
symbol_t* symbol_table_lookup(symbol_table_t* table, const char* name);

// find the first symbol matching the name in self
symbol_t* symbol_table_lookup_local(symbol_table_t* table, const char* name);

// Returns pointer to vector of symbol_t* that share the name, or nullptr if there is no match.
// NOTE: Only local searche in self, parent matches are not included.
vec_t* symbol_table_overloads(symbol_table_t* table, const char* name);

// Every symbol from src is cloned and added into dst
void symbol_table_merge(symbol_table_t* dst, symbol_table_t* src);

#endif
