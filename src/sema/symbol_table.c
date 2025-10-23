#include "symbol_table.h"

#include "common/containers/hash_table.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "sema/symbol.h"

#include <stdlib.h>

symbol_table_t* symbol_table_create(symbol_table_t* parent, scope_kind_t kind)
{
    symbol_table_t* table = malloc(sizeof(*table));
    panic_if(table == nullptr);

    *table = (symbol_table_t){
        .parent = parent,
        .kind = kind,
        .map = HASH_TABLE_INIT(vec_destroy_void),
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
        symbols = vec_create(symbol_destroy_void);
        hash_table_insert(&table->map, symbol->name, symbols);
    }
    vec_push(symbols, symbol);
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

void symbol_table_merge(symbol_table_t* dst, symbol_table_t* src)
{
    (void)dst;
    (void)src;

    panic("not implemented");
}

static void* symbol_vec_clone_wrapper(void* symbols_vec)
{
    vec_t* src_vec = symbols_vec;
    vec_t* dst_vec = vec_create(symbol_destroy_void);

    for (size_t i = 0; i < vec_size(src_vec); ++i)
    {
        symbol_t* original_symbol = vec_get(src_vec, i);
        symbol_t* cloned_symbol = symbol_clone(original_symbol);
        vec_push(dst_vec, cloned_symbol);
    }

    return dst_vec;
}

void symbol_table_clone(symbol_table_t* dst, symbol_table_t* src)
{
    panic_if(dst->map.size != 0);

    dst->kind = src->kind;
    dst->parent = src->parent;
    hash_table_deinit(&dst->map);
    hash_table_clone(&dst->map, &src->map, symbol_vec_clone_wrapper);
}
