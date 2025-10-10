#ifndef AST_TYPE__H
#define AST_TYPE__H

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

    TYPE_END,
} type_t;

typedef enum ast_type_kind
{
    AST_TYPE_INVALID,
    AST_TYPE_BUILTIN,
    AST_TYPE_USER,      // unresolved until Semantic Analysis
    AST_TYPE_POINTER,
} ast_type_kind_t;

typedef struct ast_type ast_type_t;
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
        } pointer;
    } data;
};

// Returned instance should not be edited.
ast_type_t* ast_type_from_builtin(type_t type);

// Returned instance should not be edited.
ast_type_t* ast_type_from_user(const char* type_name);

// Returned instance should not be edited.
ast_type_t* ast_type_from_token(token_t* tok);

// Returned instance should not be edited.
ast_type_t* ast_type_invalid();

bool ast_type_equal(ast_type_t* lhs, ast_type_t* rhs);

bool ast_type_is_arithmetic(ast_type_t* type);

const char* ast_type_string(ast_type_t* type);

const char* type_to_str(type_t type);

#endif
