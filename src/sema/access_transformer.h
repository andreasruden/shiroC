#ifndef SEMA_ACCESS_TRANSFORMER__H
#define SEMA_ACCESS_TRANSFORMER__H

typedef struct semantic_analyzer semantic_analyzer_t;
typedef struct ast_access_expr ast_access_expr_t;
typedef struct ast_expr ast_expr_t;
typedef struct symbol symbol_t;

// Transform an access_expr node into the appropriate semantic node (ref_expr, member_access, etc.)
//
// When in_call_context is true and the final access resolves to a method, returns the instance
// expression (everything before the method name) for method_call construction.
// out_symbol: always set to the final symbol the access resolves to (can be nullptr)
//
// Returns the transformed node, with type invalid and added errors if resolution was not possible.
// The input access_expr may be freed by the transform and it's not valid to access it after.
ast_expr_t* access_transformer_resolve(semantic_analyzer_t* sema, ast_access_expr_t* access_expr,
    bool in_call_context, symbol_t** out_symbol);

#endif
