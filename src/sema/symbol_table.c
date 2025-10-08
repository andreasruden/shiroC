#include "symbol_table.h"

#include "common/containers/hash_table.h"

#include <stdlib.h>

symbol_table_t* symbol_table_create(symbol_table_t* parent, scope_kind_t kind)
{
    symbol_table_t* table = malloc(sizeof(*table));

    *table = (symbol_table_t){
        .parent = parent,
        .kind = kind,
        .map = HASH_TABLE_INIT(symbol_destroy_void),
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
    hash_table_insert(&table->map, symbol->name, symbol);
}

symbol_t* symbol_table_lookup(symbol_table_t* table, const char* name)
{
    void* symbol = hash_table_find(&table->map, name);
    while (symbol == nullptr && table->parent != nullptr)
    {
        table = table->parent;
        symbol = hash_table_find(&table->map, name);
    }
    return symbol;
}

symbol_t* symbol_table_lookup_local(symbol_table_t* table, const char* name)
{
    return hash_table_find(&table->map, name);
}
