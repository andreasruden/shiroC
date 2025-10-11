#include "type.h"

#include "common/containers/hash_table.h"
#include "common/debug/panic.h"
#include "parser/lexer.h"

#include <string.h>

// Managed by ast_type_cache_init() and ast_type_cache_cleanup()
ast_type_t* invalid_cache = nullptr;
static ast_type_t* builtins_cache[TYPE_END] = {};
static hash_table_t* user_cache = nullptr;

static void ast_type_destroy(void* type_)
{
    ast_type_t* type = type_;
    if (type == nullptr)
        return;

    if (type->kind == AST_TYPE_BUILTIN)
    {
        panic_if(builtins_cache[type->data.builtin.type] != type);
        return;
    }

    if (type->kind == AST_TYPE_USER)
        free(type->data.user.name);

    free(type);
}

ast_type_t* ast_type_from_builtin(type_t type)
{
    ast_type_t* ast_type = builtins_cache[type];
    panic_if(ast_type == nullptr);
    return ast_type;
}

ast_type_t* ast_type_from_user(const char* type_name)
{
    if (user_cache == nullptr)
        user_cache = hash_table_create(ast_type_destroy);

    ast_type_t* ast_type = hash_table_find(user_cache, type_name);
    if (ast_type != nullptr)
        return ast_type;

    ast_type = malloc(sizeof(ast_type_t));
    panic_if(ast_type == nullptr);

    *ast_type = (ast_type_t){
        .kind = AST_TYPE_USER,
        .data.user.name = strdup(type_name),
    };
    hash_table_insert(user_cache, type_name, ast_type);

    return ast_type;
}

ast_type_t* ast_type_from_token(token_t* tok)
{
    // Resolve token -> type
    switch (tok->type)
    {
        case TOKEN_BOOL: return ast_type_from_builtin(TYPE_BOOL);
        case TOKEN_VOID: return ast_type_from_builtin(TYPE_VOID);
        case TOKEN_I8:   return ast_type_from_builtin(TYPE_I8);
        case TOKEN_I16:  return ast_type_from_builtin(TYPE_I16);
        case TOKEN_I32:  return ast_type_from_builtin(TYPE_I32);
        case TOKEN_I64:  return ast_type_from_builtin(TYPE_I64);
        case TOKEN_U8:   return ast_type_from_builtin(TYPE_U8);
        case TOKEN_U16:  return ast_type_from_builtin(TYPE_U16);
        case TOKEN_U32:  return ast_type_from_builtin(TYPE_U32);
        case TOKEN_U64:  return ast_type_from_builtin(TYPE_U64);
        case TOKEN_F32:  return ast_type_from_builtin(TYPE_F32);
        case TOKEN_F64:  return ast_type_from_builtin(TYPE_F64);

        case TOKEN_IDENTIFIER:
        return ast_type_from_user(tok->value);

        default:
            return ast_type_invalid();
    }
}

ast_type_t* ast_type_invalid()
{
    return invalid_cache;
}

bool ast_type_equal(ast_type_t* lhs, ast_type_t* rhs)
{
    if (lhs->kind != rhs->kind)
        return false;

    switch (lhs->kind)
    {
        case AST_TYPE_BUILTIN:
            return lhs->data.builtin.type == rhs->data.builtin.type;
        case AST_TYPE_USER:
            return strcmp(lhs->data.user.name, rhs->data.user.name);  // NOTE: equal name != equal type
        case AST_TYPE_INVALID:
            return true;
        case AST_TYPE_POINTER:
            break;
    }

    panic("Case %d not handled", lhs->kind);
}

bool ast_type_is_arithmetic(ast_type_t* type)
{
    if (type->kind != AST_TYPE_BUILTIN)
        return false;

    switch (type->data.builtin.type)
    {
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
        case TYPE_I64:
        case TYPE_U8:
        case TYPE_U16:
        case TYPE_U32:
        case TYPE_U64:
        case TYPE_F32:
        case TYPE_F64:
            return true;
        default:
            return false;
    }
}

bool ast_type_is_signed(ast_type_t* type)
{
    if (type->kind != AST_TYPE_BUILTIN)
        return false;

    switch (type->data.builtin.type)
    {
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
        case TYPE_I64:
            return true;
        default:
            return false;
    }
}

const char* ast_type_string(ast_type_t* type)
{
    switch (type->kind)
    {
        case AST_TYPE_BUILTIN:
            return type_to_str(type->data.builtin.type);
        case AST_TYPE_USER:
            return type->data.user.name;
        case AST_TYPE_INVALID:
            return "INVALID";
        case AST_TYPE_POINTER:
            break;
    }

    panic("Case %d not handled", type->kind);
}

const char* type_to_str(type_t type)
{
    switch (type)
    {
        case TYPE_VOID: return "void";
        case TYPE_BOOL: return "bool";
        case TYPE_I8:   return "i8";
        case TYPE_I16:  return "i16";
        case TYPE_I32:  return "i32";
        case TYPE_I64:  return "i64";
        case TYPE_U8:   return "u8";
        case TYPE_U16:  return "u16";
        case TYPE_U32:  return "u32";
        case TYPE_U64:  return "u64";
        case TYPE_F32:  return "f32";
        case TYPE_F64:  return "f64";
        case TYPE_END:  panic("Not a valid value");
    }

    panic("Case %d not handled", type);
}

__attribute__((constructor))
void ast_type_cache_init()
{
    // Invalid type
    ast_type_t* ast_type = malloc(sizeof(ast_type_t));
    panic_if(ast_type == nullptr);
    *ast_type = (ast_type_t){
        .kind = AST_TYPE_INVALID,
    };
    invalid_cache = ast_type;

    // The builtin types
    for (int type = 0; type < TYPE_END; ++type)
    {
        ast_type = malloc(sizeof(ast_type_t));
        panic_if(ast_type == nullptr);

        *ast_type = (ast_type_t){
            .kind = AST_TYPE_BUILTIN,
            .data.builtin.type = (type_t)type,
        };

        builtins_cache[type] = ast_type;
    }
}

__attribute__((destructor))
void ast_type_cache_cleanup()
{
    free(invalid_cache);
    for (int type = 0; type < TYPE_END; ++type)
        free(builtins_cache[type]);
    hash_table_destroy(user_cache);
}
