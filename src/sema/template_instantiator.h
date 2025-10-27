#ifndef SEMA_TEMPLATE_INSTANTIATOR__H
#define SEMA_TEMPLATE_INSTANTIATOR__H

#include "ast/type.h"
#include "sema/symbol.h"

typedef struct semantic_context semantic_context_t;

// Instantiate a template function with the given type arguments
// Returns the instance symbol (cached if previously instantiated)
// Returns nullptr on error (errors added to semantic context)
symbol_t* instantiate_template_function(semantic_context_t* ctx, symbol_t* template_symbol,
    ast_type_t** type_args, size_t num_type_args);

// Instantiate a template class with the given type arguments
// Returns the instance symbol (cached if previously instantiated)
// Returns nullptr on error (errors added to semantic context)
symbol_t* instantiate_template_class(semantic_context_t* ctx, symbol_t* template_symbol,
    ast_type_t** type_args, size_t num_type_args);

#endif
