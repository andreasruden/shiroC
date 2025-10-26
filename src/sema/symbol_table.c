#include "symbol_table.h"

#include "common/containers/hash_table.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "sema/symbol.h"

#include <stdlib.h>
#include <string.h>

symbol_table_t* symbol_table_create(symbol_table_t* parent, scope_kind_t kind)
{
    symbol_table_t* table = malloc(sizeof(*table));
    panic_if(table == nullptr);

    // All symbol tables own the vectors (hash table values), so always destroy them
    // The vectors themselves have delete_fn set per-scope in symbol_table_insert:
    //   - SCOPE_EXPORT: vec's delete_fn = nullptr (don't destroy symbols, just references)
    //   - Other scopes: vec's delete_fn = symbol_destroy_void (destroy owned symbols)
    vec_delete_fn destructor = vec_destroy_void;

    *table = (symbol_table_t){
        .parent = parent,
        .kind = kind,
        .map = HASH_TABLE_INIT(destructor),
    };

    return table;
}

void symbol_table_destroy(symbol_table_t* table)
{
    if (table == nullptr)
        return;

    hash_table_deinit(&table->map);
    free(table);
}

void symbol_table_destroy_void(void* table)
{
    symbol_table_destroy((symbol_table_t*)table);
}

void symbol_table_insert(symbol_table_t* table, symbol_t* symbol)
{
    vec_t* symbols = hash_table_find(&table->map, symbol->name);
    if (symbols == nullptr)
    {
        // SCOPE_EXPORT tables don't own symbols, others do
        vec_delete_fn destructor = (table->kind == SCOPE_EXPORT) ? nullptr : symbol_destroy_void;
        symbols = vec_create(destructor);
        hash_table_insert(&table->map, symbol->name, symbols);
    }
    vec_push(symbols, symbol);
}

symbol_table_t* symbol_table_parent_with_symbol(symbol_table_t* table, const char* name)
{
    symbol_table_t* itr = table;
    while (itr != nullptr)
    {
        if (symbol_table_lookup_local(itr, name) != nullptr)
            return itr;
        itr = itr->parent;
    }
    return nullptr;
}

symbol_t* symbol_table_lookup(symbol_table_t* table, const char* name)
{
    vec_t* symbols = hash_table_find(&table->map, name);
    while (symbols == nullptr && table->parent != nullptr)
    {
        table = table->parent;
        symbols = hash_table_find(&table->map, name);
    }
    return symbols ? vec_get(symbols, 0) : nullptr;
}

symbol_t* symbol_table_lookup_local(symbol_table_t* table, const char* name)
{
    vec_t* symbols = hash_table_find(&table->map, name);
    return symbols ? vec_get(symbols, 0) : nullptr;
}

vec_t* symbol_table_overloads(symbol_table_t* table, const char* name)
{
    return hash_table_find(&table->map, name);
}

void symbol_table_import(symbol_table_t* dst, symbol_table_t* src, symbol_t* imported_namespace)
{
    hash_table_iter_t itr;
    for (hash_table_iter_init(&itr, &src->map); hash_table_iter_has_elem(&itr); hash_table_iter_next(&itr))
    {
        hash_table_entry_t* entry = hash_table_iter_current(&itr);
        vec_t* symbols = entry->value;

        for (size_t i = 0; i < vec_size(symbols); ++i)
        {
            symbol_t* symbol = vec_get(symbols, i);
            symbol_t* cloned_symbol = symbol_clone(symbol, false, imported_namespace);
            symbol_table_insert(dst, cloned_symbol);
        }
    }
}
