#ifndef AST_TYPE__H
#define AST_TYPE__H

#include "common/containers/vec.h"
#include <stdint.h>
#include <stddef.h>

typedef enum type
{
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_I64,
    TYPE_ISIZE,
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,
    TYPE_USIZE,
    TYPE_F32,
    TYPE_F64,
    TYPE_STRING,
    TYPE_NULL,
    TYPE_UNINIT,

    TYPE_END,
} type_t;

typedef enum ast_type_kind
{
    AST_TYPE_INVALID,
    AST_TYPE_BUILTIN,
    AST_TYPE_CLASS,             // unresolved until Semantic Analysis
    AST_TYPE_POINTER,
    AST_TYPE_ARRAY,             // unresolved until Semantic Analysis
    AST_TYPE_HEAP_ARRAY,        // TODO: Dyn Array would probably be a better name (or flexible, ...?), heap gives wrong idea
    AST_TYPE_VIEW,
    AST_TYPE_VARIABLE,          // Type parameter (e.g., T in a template)
    AST_TYPE_TEMPLATE_INSTANCE, // Instantiated template type (e.g., MyType<i32>)
} ast_type_kind_t;

typedef enum ast_coercion_kind
{
    COERCION_INVALID,
    COERCION_EQUAL,         // no coercion needed, already equal
    COERCION_ALWAYS,        // coercion is always OK, e.g. array -> view
    COERCION_WIDEN,         // smaller int/float -> bigger int/float of same signedness
    COERCION_SIGNEDNESS,    // integer changes sign, can also include widening
    COERCION_INIT,          // only valid during initialization
} ast_coercion_kind_t;

typedef enum ast_trait
{
    TRAIT_COPYABLE,
    TRAIT_EXPLICIT_DESTRUCTOR,
    TRAIT_ARITHMETIC,     // Type supports arithmetic operations (+, -, *, /, %)
    TRAIT_COMPARABLE,     // Type supports comparison operations (<, >, <=, >=)
    TRAIT_SUBSCRIPTABLE,  // Type supports subscript operations ([])
    TRAIT_DEREFERENCEABLE, // Type supports dereference operation (*)

    TRAIT_END,
} ast_trait_t;

typedef struct ast_type ast_type_t;
typedef struct ast_expr ast_expr_t;
typedef struct symbol symbol_t;
typedef struct token token_t;

// Instances of ast_type_t should always be assumed const and not edited.
struct ast_type
{
    ast_type_kind_t kind;
    bool traits[TRAIT_END];

    // Kind specific data
    union
    {
        struct
        {
            type_t type;
        } builtin;

        struct
        {
            char* name;                 // nullptr if type is resolved
            symbol_t* class_symbol;     // nullptr until decl_collector (symbol owned by semantic_context)

            // Fields only used by AST_TYPE_TEMPLATE_INSTANCE:
            vec_t* type_arguments;      // vec<ast_type_t*> array of type arguments (nullptr if not template)
            symbol_t* template_symbol;  // nullptr if not template
            char* str_repr;
        } class;

        struct
        {
            ast_type_t* pointee;
            char* str_repr;
        } pointer;

        struct
        {
            ast_type_t* element_type;
            bool size_known;
            union
            {
                ast_expr_t* size_expr; // result of parsing (NOTE: before SEMA array types never compare equally)
                size_t size;           // calcualted by SEMA from size_expr
            };
            char* str_repr;
        } array;

        struct
        {
            ast_type_t* element_type;
            char* str_repr;
        } heap_array;

        struct
        {
            ast_type_t* element_type;
            char* str_repr;
        } view;

        struct
        {
            char* name;  // type parameter name (e.g., "T")
        } type_variable;
    } data;
};

// Returned instance should not be edited.
ast_type_t* ast_type_builtin(type_t type);

// Returned instance should not be edited.
ast_type_t* ast_type_user(symbol_t* class_symbol);

// Returned instance should not be edited.
ast_type_t* ast_type_user_unresolved(const char* type_name);

// Create unresolved user type with type arguments (for template instantiation during parsing)
// Returned instance should not be edited.
ast_type_t* ast_type_user_unresolved_with_args(const char* type_name, ast_type_t** type_args, size_t num_type_args);

// Returned instance should not be edited.
ast_type_t* ast_type_pointer(ast_type_t* pointee);

// Returned instance should not be edited.
ast_type_t* ast_type_array(ast_type_t* element_type, size_t size);

// Returned instance should not be edited.
ast_type_t* ast_type_array_size_unresolved(ast_type_t* element_type, ast_expr_t* size_expr);

// Returned instance should not be edited.
ast_type_t* ast_type_heap_array(ast_type_t* element_type);

// Returned instance should not be edited.
ast_type_t* ast_type_view(ast_type_t* element_type);

// Returned instance should not be edited.
ast_type_t* ast_type_invalid();

// Returned instance should not be edited.
ast_type_t* ast_type_variable(const char* name);

// Returned instance should not be edited.
ast_type_t* ast_type_template_instance(symbol_t* template_symbol, vec_t* type_args);

// Returned instance should not be edited.
ast_type_t* ast_type_from_token(token_t* tok);

void ast_type_set_trait(ast_type_t* type, ast_trait_t trait);

void ast_type_clear_trait(ast_type_t* type, ast_trait_t trait);

bool ast_type_has_trait(ast_type_t* type, ast_trait_t trait);

bool ast_type_is_arithmetic(ast_type_t* type);

bool ast_type_is_integer(ast_type_t* type);

bool ast_type_is_real(ast_type_t* type);

bool ast_type_is_signed(ast_type_t* type);

size_t ast_type_sizeof(ast_type_t* type);

bool ast_type_has_equality(ast_type_t* type);

bool ast_type_is_instantiable(ast_type_t* type);

ast_coercion_kind_t ast_type_can_coerce(ast_type_t* from, ast_type_t* to);

const char* ast_type_string(ast_type_t* type);

const char* type_to_str(type_t type);

// Resets all non-builtin type caches to default state.
// Use to prevent dangling symbol pointers in e.g. unit tests or separate compilations.
// TODO: This seems to imply we would want some compilation context perhaps.
void ast_type_cache_reset();

#endif
