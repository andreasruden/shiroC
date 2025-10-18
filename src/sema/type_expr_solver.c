#include "type_expr_solver.h"

#include "ast/expr/int_lit.h"
#include "ast/type.h"
#include "common/debug/panic.h"
#include "sema/semantic_context.h"

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

ast_type_t* type_expr_solver_solve(semantic_context_t* ctx, ast_type_t* type, void* node)
{
    // TODO: We could consider cleaning up the no-longer used types already (when the inner type had expressions
    //       that needed resolving, the previous type is replaced and no longer referenced anywhere: the unresolved
    //       types are all unique)

    ast_type_t* inner_type = nullptr;
    switch (type->kind)
    {
        case AST_TYPE_ARRAY:
            inner_type = type_expr_solver_solve(ctx, type->data.array.element_type, node);
            if (!type->data.array.size_known)
                return solve_array_size(ctx, type, inner_type, node);
            if (inner_type == type->data.array.element_type)
                return type;  // no changes
            return ast_type_array(inner_type, type->data.array.size_known);
        case AST_TYPE_HEAP_ARRAY:
            inner_type = type_expr_solver_solve(ctx, type->data.heap_array.element_type, node);
            if (inner_type == type->data.heap_array.element_type)
                return type;  // no changes
            return ast_type_heap_array(inner_type);
        case AST_TYPE_VIEW:
            inner_type = type_expr_solver_solve(ctx, type->data.view.element_type, node);
            if (inner_type == type->data.heap_array.element_type)
                return type;  // no changes
            return ast_type_view(inner_type);
        case AST_TYPE_POINTER:
            inner_type = type_expr_solver_solve(ctx, type->data.pointer.pointee, node);
            if (inner_type == type->data.heap_array.element_type)
                return type;  // no changes
            return ast_type_pointer(inner_type);
        default:
            break;
    }
    return type;
}
