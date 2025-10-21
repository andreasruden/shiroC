#include "symbol.h"
#include "ast/decl/param_decl.h"
#include "common/debug/panic.h"
#include "symbol_table.h"
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
        case SYMBOL_METHOD:
            [[fallthrough]];
        case SYMBOL_FUNCTION:
            symbol->data.function.parameters = VEC_INIT(nullptr);
            break;
        case SYMBOL_CLASS:
            symbol->data.class.members = HASH_TABLE_INIT(nullptr);
            symbol->data.class.methods = symbol_table_create(nullptr, SCOPE_CLASS);
            break;
        default:
            break;
    }

    return symbol;
}

symbol_t* symbol_clone(symbol_t* source)
{
    symbol_t* new_symb = symbol_create(source->name, source->kind, source->ast);

    switch (source->kind)
    {
        case SYMBOL_METHOD:
        case SYMBOL_FUNCTION:
            for (size_t i = 0; i < vec_size(&source->data.function.parameters); ++i)
            {
                ast_param_decl_t* param = vec_get(&source->data.function.parameters, i);
                vec_push(&new_symb->data.function.parameters, param);
            }
            new_symb->data.function.return_type = source->data.function.return_type;
            break;
        case SYMBOL_CLASS:
            panic("Cloning class symbols is not implemented");
            break;
        default:
            break;
    }

    return new_symb;
}

void symbol_destroy(symbol_t* symbol)
{
    if (symbol == nullptr)
        return;

    switch (symbol->kind)
    {
        case SYMBOL_METHOD:
            [[fallthrough]];
        case SYMBOL_FUNCTION:
            vec_deinit(&symbol->data.function.parameters);
            break;
        case SYMBOL_CLASS:
            hash_table_deinit(&symbol->data.class.members);
            symbol_table_destroy(symbol->data.class.methods);
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
