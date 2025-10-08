#include "type.h"

#include "common/debug/panic.h"
#include "lexer.h"

#include <string.h>

ast_type_t* ast_type_from_builtin(type_t type)
{
    ast_type_t* ast_type = malloc(sizeof(ast_type_t));
    panic_if(ast_type == nullptr);

    *ast_type = (ast_type_t){
        .kind = AST_TYPE_BUILTIN,
        .data.builtin.type = type,
    };

    return ast_type;
}

ast_type_t* ast_type_from_token(token_t* tok)
{
    ast_type_kind_t kind = AST_TYPE_BUILTIN;
    type_t builtin_type = TYPE_VOID;
    ast_type_t* ast_type = malloc(sizeof(ast_type_t));
    panic_if(ast_type == nullptr);

    // Resolve token -> type
    switch (tok->type)
    {
        case TOKEN_BOOL:       builtin_type = TYPE_BOOL; break;
        case TOKEN_VOID:       builtin_type = TYPE_VOID; break;
        case TOKEN_I8:         builtin_type = TYPE_I8; break;
        case TOKEN_I16:        builtin_type = TYPE_I16; break;
        case TOKEN_I32:        builtin_type = TYPE_I32; break;
        case TOKEN_I64:        builtin_type = TYPE_I64; break;
        case TOKEN_U8:         builtin_type = TYPE_U8; break;
        case TOKEN_U16:        builtin_type = TYPE_U16; break;
        case TOKEN_U32:        builtin_type = TYPE_U32; break;
        case TOKEN_U64:        builtin_type = TYPE_U64; break;
        case TOKEN_F32:        builtin_type = TYPE_F32; break;
        case TOKEN_F64:        builtin_type = TYPE_F64; break;
        case TOKEN_IDENTIFIER: kind = AST_TYPE_USER; break;
        default:
            return ast_type_create_invalid();
    }

    if (kind == AST_TYPE_BUILTIN)
    {
        *ast_type = (ast_type_t){
            .kind = kind,
            .data.builtin.type = builtin_type,
        };
    }
    else if (kind == AST_TYPE_USER)
    {
        *ast_type = (ast_type_t){
            .kind = kind,
            .data.user.name = strdup(tok->value),
        };
    }
    else
    {
        panic("Not implemented yet (%d)", kind);
    }

    return ast_type;
}

ast_type_t* ast_type_create_invalid()
{
    ast_type_t* ast_type = malloc(sizeof(ast_type_t));
    panic_if(ast_type == nullptr);

    *ast_type = (ast_type_t){
        .kind = AST_TYPE_INVALID,
    };

    return ast_type;
}

void ast_type_destroy(ast_type_t* type)
{
    if (type == nullptr)
        return;

    if (type->kind == AST_TYPE_USER)
        free(type->data.user.name);

    free(type);
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
    }

    panic("Case %d not handled", type);
}
