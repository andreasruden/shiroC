#include "symbol.h"
#include "ast/type.h"
#include "common/containers/hash_table.h"
#include "common/containers/vec.h"

#include <stdlib.h>
#include <string.h>

symbol_t* symbol_create(const char* name, symbol_kind_t kind, void* ast)
{
    symbol_t* symbol = malloc(sizeof(*symbol));

    *symbol = (symbol_t){
        .name = strdup(name),
        .kind = kind,
        .ast = ast,
    };

    switch (kind)
    {
        case SYMBOL_FUNCTION:
            symbol->data.function.parameters = VEC_INIT(nullptr);
            break;
        case SYMBOL_CLASS:
            symbol->data.class.members = HASH_TABLE_INIT(nullptr);
            symbol->data.class.methods = HASH_TABLE_INIT(nullptr);
            break;
        default:
            break;
    }

    return symbol;
}

void symbol_destroy(symbol_t* symbol)
{
    if (symbol == nullptr)
        return;

    switch (symbol->kind)
    {
        case SYMBOL_FUNCTION:
            vec_deinit(&symbol->data.function.parameters);
            break;
        case SYMBOL_CLASS:
            hash_table_deinit(&symbol->data.class.members);
            hash_table_deinit(&symbol->data.class.methods);
            break;
        default:
            break;
    }

    free(symbol->name);
    free(symbol);
}

void symbol_destroy_void(void* symbol)
{
    symbol_destroy((symbol_t*)symbol);
}
