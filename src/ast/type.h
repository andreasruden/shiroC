#ifndef AST_TYPE__H
#define AST_TYPE__H

#include <stdint.h>

typedef enum type
{
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_I64,
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,
    TYPE_F32,
    TYPE_F64,
    TYPE_NULL,

    TYPE_END,
} type_t;

typedef enum ast_type_kind
{
    AST_TYPE_INVALID,
    AST_TYPE_BUILTIN,
    AST_TYPE_USER,      // unresolved until Semantic Analysis
    AST_TYPE_POINTER,
    AST_TYPE_ARRAY,
    AST_TYPE_HEAP_ARRAY,  // TODO: Dyn Array would probably be a better name (or flexible, ...?), heap gives wrong idea
    AST_TYPE_VIEW,
} ast_type_kind_t;

typedef enum ast_coercion_kind
{
    COERCION_INVALID,
    COERCION_EQUAL,         // no coercion needed, already equal
    COERCION_ALWAYS,        // coercion is always OK, e.g. array -> view
    COERCION_WIDEN,         // smaller int/float -> bigger int/float of same signedness
} ast_coercion_kind_t;

typedef struct ast_type ast_type_t;
typedef struct ast_expr ast_expr_t;
typedef struct token token_t;

// Instances of ast_type_t should always be assumed const and not edited.
struct ast_type
{
    ast_type_kind_t kind;

    // Kind specific data
    union
    {
        struct
        {
            type_t type;
        } builtin;

        struct
        {
            char* name;
        } user;

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
                intptr_t size;         // calcualted by SEMA from size_expr
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
    } data;
};

// Returned instance should not be edited.
ast_type_t* ast_type_builtin(type_t type);

// Returned instance should not be edited.
ast_type_t* ast_type_user(const char* type_name);

// Returned instance should not be edited.
ast_type_t* ast_type_pointer(ast_type_t* pointee);

// Returned instance should not be edited.
ast_type_t* ast_type_array(ast_type_t* element_type, intptr_t size);

// Returned instance should not be edited.
ast_type_t* ast_type_array_size_unresolved(ast_type_t* element_type, ast_expr_t* size_expr);

// Returned instance should not be edited.
ast_type_t* ast_type_heap_array(ast_type_t* element_type);

// Returned instance should not be edited.
ast_type_t* ast_type_view(ast_type_t* element_type);

// Returned instance should not be edited.
ast_type_t* ast_type_invalid();

// Returned instance should not be edited.
ast_type_t* ast_type_from_token(token_t* tok);

bool ast_type_is_arithmetic(ast_type_t* type);

bool ast_type_is_signed(ast_type_t* type);

bool ast_type_has_equality(ast_type_t* type);

bool ast_type_is_instantiable(ast_type_t* type);

ast_coercion_kind_t ast_type_can_coerce(ast_type_t* from, ast_type_t* to);

const char* ast_type_string(ast_type_t* type);

const char* type_to_str(type_t type);

#endif
