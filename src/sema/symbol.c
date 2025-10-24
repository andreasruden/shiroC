#include "symbol.h"
#include "ast/node.h"
#include "ast/util/cloner.h"
#include "common/containers/string.h"
#include "common/debug/panic.h"
#include "symbol_table.h"
#include "common/containers/hash_table.h"
#include "common/containers/vec.h"

#include <stdlib.h>
#include <string.h>

static void fill_in_fully_qualified_name(symbol_t* symbol, const char* project, const char* module,
    const char* class);

symbol_t* symbol_create(const char* name, symbol_kind_t kind, void* ast, const char* project, const char* module,
    const char* class)
{
    symbol_t* symbol = malloc(sizeof(*symbol));

    *symbol = (symbol_t){
        .name = strdup(name),
        .kind = kind,
        .ast = ast,
        .source_project = project ? strdup(project) : nullptr,
        .source_module = module ? strdup(module) : nullptr,
        .class_scope = class ? strdup(class) : nullptr,
    };

    switch (kind)
    {
        case SYMBOL_METHOD:
            [[fallthrough]];
        case SYMBOL_FUNCTION:
            symbol->data.function.parameters = VEC_INIT(symbol_destroy_void);
            break;
        case SYMBOL_CLASS:
            symbol->data.class.members = HASH_TABLE_INIT(symbol_destroy_void);
            symbol->data.class.methods = symbol_table_create(nullptr, SCOPE_CLASS);
            break;
        default:
            break;
    }

    fill_in_fully_qualified_name(symbol, project, module, class);

    return symbol;
}

static void* member_clone_with_ast(void* member)
{
    return symbol_clone(member, true);
}

static void* member_clone_without_ast(void* member)
{
    return symbol_clone(member, false);
}

symbol_t* symbol_clone(symbol_t* source, bool include_ast)
{
    symbol_t* new_symb = symbol_create(source->name, source->kind, include_ast ? source->ast : nullptr,
        source->source_project, source->source_module, source->class_scope);
    new_symb->type = source->type;

    switch (source->kind)
    {
        case SYMBOL_METHOD:
        case SYMBOL_FUNCTION:
            for (size_t i = 0; i < vec_size(&source->data.function.parameters); ++i)
            {
                symbol_t* param = vec_get(&source->data.function.parameters, i);
                vec_push(&new_symb->data.function.parameters, symbol_clone(param, include_ast));
            }
            new_symb->data.function.return_type = source->data.function.return_type;
            new_symb->data.function.overload_index = source->data.function.overload_index;
            break;
        case SYMBOL_CLASS:
            hash_table_deinit(&new_symb->data.class.members);
            hash_table_clone(&new_symb->data.class.members, &source->data.class.members,
                include_ast ? member_clone_with_ast : member_clone_without_ast);
            symbol_table_clone(new_symb->data.class.methods, source->data.class.methods, include_ast);
            break;
        case SYMBOL_MEMBER:
            if (new_symb->data.member.default_value != nullptr)
                new_symb->data.member.default_value = ast_expr_clone(new_symb->data.member.default_value);
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
        case SYMBOL_MEMBER:
            ast_node_destroy(symbol->data.member.default_value);
            break;
        default:
            break;
    }

    free(symbol->name);
    free(symbol->source_project);
    free(symbol->source_module);
    free(symbol->class_scope);
    free(symbol->fully_qualified_name);
    free(symbol);
}

void symbol_destroy_void(void* symbol)
{
    symbol_destroy((symbol_t*)symbol);
}

static void fill_in_fully_qualified_name(symbol_t* symbol, const char* project, const char* module,
    const char* class)
{
    if (symbol->kind != SYMBOL_FUNCTION && symbol->kind != SYMBOL_CLASS && symbol->kind != SYMBOL_METHOD)
        return;

    string_t str = STRING_INIT;

    if (symbol->source_project)
    {
        string_append_cstr(&str, project);
        string_append_char(&str, '.');
        string_append_cstr(&str, module);
        string_append_char(&str, '.');
    }

    if (symbol->kind == SYMBOL_METHOD)
    {
        panic_if(class == nullptr);
        string_append_cstr(&str, class);
        string_append_char(&str, '.');
    }

    string_append_cstr(&str, symbol->name);
    symbol->fully_qualified_name = string_release(&str);
}
