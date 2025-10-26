#include "type_resolver.h"

#include "ast/expr/int_lit.h"
#include "ast/type.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "common/util/ssprintf.h"
#include "sema/semantic_context.h"
#include "sema/symbol_table.h"
#include <string.h>

static ast_type_t* solve_array_size(semantic_context_t* ctx, ast_type_t* type, ast_type_t* inner_type, void* node)
{
    panic_if(type->kind != AST_TYPE_ARRAY || type->data.array.size_known);

    // TODO: More advanced resolution
    if (AST_KIND(type->data.array.size_expr) != AST_EXPR_INT_LIT)
    {
        semantic_context_add_error(ctx, node, "invalid array-size expression");
        return ast_type_invalid();
    }

    if (((ast_int_lit_t*)type->data.array.size_expr)->has_minus_sign)
    {
        semantic_context_add_error(ctx, node, "array-size must be > 0");
        return ast_type_invalid();
    }

    size_t size = ((ast_int_lit_t*)type->data.array.size_expr)->value.as_unsigned;

    return ast_type_array(inner_type, size);
}

static symbol_t* lookup_class_symbol(semantic_context_t* ctx, const char* name, void* node)
{
    vec_t* overloads = symbol_table_overloads(ctx->global, name);
    if (overloads == nullptr || vec_size(overloads) == 0)
    {
        semantic_context_add_error(ctx, node, ssprintf("undefined type '%s'", name));
        return nullptr;
    }

    // NOTE: decl collector will fail a redefine of a type in the same module; so here we just worry about ambiguity
    //       across modules: ambiguity is okay if one name is from our module, but an error if both names are imports.
    symbol_t* selected_class = nullptr;
    for (size_t i = 0; i < vec_size(overloads); ++i)
    {
        symbol_t* class_symb = vec_get(overloads, i);
        bool is_ours = class_symb->parent_namespace == nullptr || class_symb->parent_namespace == ctx->module_namespace;
        if (is_ours)
        {
            selected_class = class_symb;
            break;
        }
        else if (selected_class == nullptr)
        {
            selected_class = class_symb;
            continue;
        }
        else
        {
            semantic_context_add_error(ctx, node, "ambiguous name resolution");
            break;
        }
    }
    return selected_class;
}

ast_type_t* type_resolver_solve(semantic_context_t* ctx, ast_type_t* type, void* node)
{
    // TODO: We could consider cleaning up the no-longer used types already (when the inner type had expressions
    //       that needed resolving, the previous type is replaced and no longer referenced anywhere: the unresolved
    //       types are all unique)

    ast_type_t* inner_type = nullptr;
    switch (type->kind)
    {
        case AST_TYPE_ARRAY:
            inner_type = type_resolver_solve(ctx, type->data.array.element_type, node);
            if (!type->data.array.size_known)
                return solve_array_size(ctx, type, inner_type, node);
            if (inner_type == type->data.array.element_type)
                return type;  // no changes
            return ast_type_array(inner_type, type->data.array.size_known);
        case AST_TYPE_HEAP_ARRAY:
            inner_type = type_resolver_solve(ctx, type->data.heap_array.element_type, node);
            if (inner_type == type->data.heap_array.element_type)
                return type;  // no changes
            return ast_type_heap_array(inner_type);
        case AST_TYPE_VIEW:
            inner_type = type_resolver_solve(ctx, type->data.view.element_type, node);
            if (inner_type == type->data.heap_array.element_type)
                return type;  // no changes
            return ast_type_view(inner_type);
        case AST_TYPE_POINTER:
            inner_type = type_resolver_solve(ctx, type->data.pointer.pointee, node);
            if (inner_type == type->data.heap_array.element_type)
                return type;  // no changes
            return ast_type_pointer(inner_type);
        case AST_TYPE_USER:
            if (type->data.user.class_symbol == nullptr)
            {
                symbol_t* class_symb = lookup_class_symbol(ctx, type->data.user.name, node);
                if (class_symb == nullptr)
                    return ast_type_invalid();
                else
                    return ast_type_user(class_symb);
            }
            return type;
        default:
            break;
    }
    return type;
}
