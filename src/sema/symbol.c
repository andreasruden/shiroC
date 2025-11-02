#include "symbol.h"
#include "ast/node.h"
#include "ast/type.h"
#include "ast/util/cloner.h"
#include "common/containers/hash_table.h"
#include "common/containers/string.h"
#include "common/debug/panic.h"
#include "symbol_table.h"
#include "common/containers/vec.h"

#include <stdlib.h>
#include <string.h>

static void fill_in_fully_qualified_name(symbol_t* symbol);

symbol_t* symbol_create(const char* name, symbol_kind_t kind, void* ast, symbol_t* parent_namespace)
{
    symbol_t* symbol = malloc(sizeof(*symbol));

    *symbol = (symbol_t){
        .name = strdup(name),
        .kind = kind,
        .ast = ast,
        .parent_namespace = parent_namespace,
        .type = ast_type_invalid(),
    };

    switch (kind)
    {
        case SYMBOL_METHOD:
            [[fallthrough]];
        case SYMBOL_FUNCTION:
            symbol->data.function.parameters = VEC_INIT(symbol_destroy_void);
            break;
        case SYMBOL_CLASS:
            symbol->data.class.symbols = symbol_table_create(nullptr, SCOPE_CLASS);
            break;
        case SYMBOL_NAMESPACE:
            symbol->data.namespace.exports = symbol_table_create(nullptr, SCOPE_EXPORT);
            break;
        case SYMBOL_TEMPLATE_CLASS:
            symbol->data.template_class.cls.symbols = symbol_table_create(nullptr, SCOPE_CLASS);
            symbol->data.template_class.type_parameters = VEC_INIT(nullptr);
            symbol->data.template_class.instantiations = VEC_INIT(nullptr);
            symbol->data.template_class.template_ast = nullptr;
            break;
        case SYMBOL_TEMPLATE_FN:
            symbol->data.template_fn.fn.parameters = VEC_INIT(symbol_destroy_void);
            symbol->data.template_fn.type_parameters = VEC_INIT(nullptr);
            symbol->data.template_fn.instantiations = VEC_INIT(nullptr);
            symbol->data.template_fn.template_ast = nullptr;
            break;
        case SYMBOL_TEMPLATE_CLASS_INST:
            symbol->data.template_class_inst.cls.symbols = symbol_table_create(nullptr, SCOPE_CLASS);
            symbol->data.template_class_inst.template_symbol = nullptr;
            symbol->data.template_class_inst.type_arguments = VEC_INIT(nullptr);
            symbol->data.template_class_inst.instantiated_ast = nullptr;
            break;
        case SYMBOL_TEMPLATE_FN_INST:
            symbol->data.template_fn_inst.fn.parameters = VEC_INIT(symbol_destroy_void);
            symbol->data.template_fn_inst.template_symbol = nullptr;
            symbol->data.template_fn_inst.type_arguments = VEC_INIT(nullptr);
            symbol->data.template_fn_inst.instantiated_ast = nullptr;
            break;
        default:
            break;
    }

    if (parent_namespace != nullptr && parent_namespace->kind == SYMBOL_NAMESPACE)
        symbol_table_insert(parent_namespace->data.namespace.exports, symbol);

    fill_in_fully_qualified_name(symbol);

    return symbol;
}

symbol_t* symbol_clone(symbol_t* source, bool include_ast, symbol_t* parent_namespace)
{
    symbol_t* new_symb = symbol_create(source->name, source->kind, include_ast ? source->ast : nullptr,
        parent_namespace);
    new_symb->type = source->type;

    switch (source->kind)
    {
        case SYMBOL_METHOD:
        case SYMBOL_FUNCTION:
        case SYMBOL_TRAIT_IMPL:
            for (size_t i = 0; i < vec_size(&source->data.function.parameters); ++i)
            {
                symbol_t* param = vec_get(&source->data.function.parameters, i);
                vec_push(&new_symb->data.function.parameters, symbol_clone(param, include_ast, nullptr));
            }
            new_symb->data.function.return_type = source->data.function.return_type;
            new_symb->data.function.overload_index = source->data.function.overload_index;
            new_symb->data.function.extern_abi = source->data.function.extern_abi ?
                strdup(source->data.function.extern_abi) : nullptr;
            break;
        case SYMBOL_CLASS:
        {
            hash_table_iter_t itr;
            for (hash_table_iter_init(&itr, &source->data.class.symbols->map); hash_table_iter_has_elem(&itr);
                hash_table_iter_next(&itr))
            {
                vec_t* overloads = hash_table_iter_current(&itr)->value;
                for (size_t i = 0; i < vec_size(overloads); ++i)
                {
                    symbol_t* symb = vec_get(overloads, i);
                    symbol_t* cloned = symbol_clone(symb, include_ast, nullptr);
                    cloned->parent_namespace = new_symb;
                    fill_in_fully_qualified_name(cloned);
                    symbol_table_insert(new_symb->data.class.symbols, cloned);
                }
            }
            break;
        }
        case SYMBOL_MEMBER:
            if (new_symb->data.member.default_value != nullptr)
                new_symb->data.member.default_value = ast_expr_clone(new_symb->data.member.default_value);
            break;
        case SYMBOL_PARAMETER:
        case SYMBOL_VARIABLE:
        case SYMBOL_TYPE_PARAMETER:
            break;  // nothing to do, normal clone enough
        default:
            panic("symbol kind %d cloning not implemented", source->kind);
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
            free(symbol->data.function.extern_abi);
            break;
        case SYMBOL_CLASS:
            symbol_table_destroy(symbol->data.class.symbols);
            break;
        case SYMBOL_MEMBER:
            ast_node_destroy(symbol->data.member.default_value);
            break;
        case SYMBOL_NAMESPACE:
            symbol_table_destroy(symbol->data.namespace.exports);
            break;
        case SYMBOL_TEMPLATE_CLASS:
            vec_deinit(&symbol->data.template_class.type_parameters);
            vec_deinit(&symbol->data.template_class.instantiations);
            symbol_table_destroy(symbol->data.template_class.cls.symbols);
            break;
        case SYMBOL_TEMPLATE_FN:
            vec_deinit(&symbol->data.template_fn.type_parameters);
            vec_deinit(&symbol->data.template_fn.instantiations);
            vec_deinit(&symbol->data.template_fn.fn.parameters);
            break;
        case SYMBOL_TEMPLATE_CLASS_INST:
            vec_deinit(&symbol->data.template_class_inst.type_arguments);
            symbol_table_destroy(symbol->data.template_class_inst.cls.symbols);
            break;
        case SYMBOL_TEMPLATE_FN_INST:
            vec_deinit(&symbol->data.template_fn_inst.type_arguments);
            vec_deinit(&symbol->data.template_fn_inst.fn.parameters);
            break;
        default:
            break;
    }

    free(symbol->name);
    free(symbol->fully_qualified_name);
    free(symbol);
}

void symbol_destroy_void(void* symbol)
{
    symbol_destroy((symbol_t*)symbol);
}

static void fill_in_fully_qualified_name(symbol_t* symbol)
{
    free(symbol->fully_qualified_name);  // Free the old name before reassigning

    string_t str = STRING_INIT;

    if (symbol->parent_namespace != nullptr)
    {
        switch (symbol->parent_namespace->kind)
        {
            case SYMBOL_NAMESPACE:
            case SYMBOL_CLASS:
            case SYMBOL_TEMPLATE_CLASS:
            case SYMBOL_TEMPLATE_CLASS_INST:
                string_append_cstr(&str, symbol->parent_namespace->fully_qualified_name);
                string_append_char(&str, '.');
                break;
            default:
                panic("Invalid namespace kind %d", symbol->parent_namespace->kind);
        }
    }

    string_append_cstr(&str, symbol->name);

    symbol->fully_qualified_name = string_release(&str);
}
