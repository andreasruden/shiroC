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

ast_type_t* ast_type_from_builtin(type_t type);

ast_type_t* ast_type_from_token(token_t* tok);

ast_type_t* ast_type_create_invalid();

void ast_type_destroy(ast_type_t* type);

bool ast_type_equal(ast_type_t* lhs, ast_type_t* rhs);

const char* ast_type_string(ast_type_t* type);

const char* type_to_str(type_t type);

#endif
