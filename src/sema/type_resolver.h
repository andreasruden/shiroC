#ifndef SEMA_TYPE_EXPR_SOLVER__H
#define SEMA_TYPE_EXPR_SOLVER__H

#include "ast/type.h"
#include "sema/semantic_context.h"

// Some types are unresolved by the parser, and need to be resolved with more context.
// This resolve those, returning the resolved type. If an error is encounted, the returned type is
// ast_type_invalid().
// If emit_errors is true, errors are added to the semantic context when resolution fails.
// If emit_errors is false, failures are silent (useful during declaration collection).
ast_type_t* type_resolver_solve(semantic_context_t* ctx, ast_type_t*, void* node, bool emit_errors);

#endif
