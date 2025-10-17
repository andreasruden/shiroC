#include "type.h"

#include "ast/expr/expr.h"
#include "ast/node.h"
#include "common/containers/hash_table.h"
#include "common/containers/string.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "common/util/ssprintf.h"
#include "parser/lexer.h"

#include <string.h>

// Managed by ast_type_cache_init() and ast_type_cache_cleanup()
ast_type_t* invalid_cache = nullptr;
static ast_type_t* builtins_cache[TYPE_END] = {};
static hash_table_t* user_cache = nullptr;
static hash_table_t* pointer_cache = nullptr;  // Key: pointee type address -> Value: pointer type
static hash_table_t* fixed_array_cache = nullptr;  // Key: element type address, size -> Value: array type
static hash_table_t* heap_array_cache = nullptr;  // Key: element type address -> Value: array type
static hash_table_t* view_cache = nullptr;  // Key: element type address -> Value: view type
static vec_t* gc_array = nullptr; // unresolved fixed size arrays for garbage collection

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
    else if (type->kind == AST_TYPE_POINTER)
        free(type->data.pointer.str_repr);
    else if (type->kind == AST_TYPE_ARRAY)
    {
        if (!type->data.array.size_known)
            ast_node_destroy(type->data.array.size_expr);
        free(type->data.array.str_repr);
    }
    else if (type->kind == AST_TYPE_HEAP_ARRAY)
        free(type->data.heap_array.str_repr);
    else if (type->kind == AST_TYPE_VIEW)
        free(type->data.view.str_repr);

    free(type);
}

ast_type_t* ast_type_builtin(type_t type)
{
    ast_type_t* ast_type = builtins_cache[type];
    panic_if(ast_type == nullptr);
    return ast_type;
}

ast_type_t* ast_type_user(const char* type_name)
{
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

ast_type_t* ast_type_pointer(ast_type_t* pointee)
{
    ast_type_t* pointer = hash_table_find(pointer_cache, ssprintf("%p", pointee));
    if (pointer != nullptr)
        return pointer;

    pointer = malloc(sizeof(*pointer));

    *pointer = (ast_type_t){
        .kind = AST_TYPE_POINTER,
        .data.pointer.pointee = pointee,
    };

    hash_table_insert(pointer_cache, ssprintf("%p", pointee), pointer);
    return pointer;
}

ast_type_t* ast_type_array(ast_type_t* element_type, size_t size)
{
    const char* key = ssprintf("%p, %lld", element_type, (long long)size);
    ast_type_t* array = hash_table_find(fixed_array_cache, key);
    if (array != nullptr)
        return array;

    array = malloc(sizeof(*array));

    *array = (ast_type_t){
        .kind = AST_TYPE_ARRAY,
        .data.array.element_type = element_type,
        .data.array.size_known = true,
        .data.array.size = size,
    };

    hash_table_insert(fixed_array_cache, key, array);
    return array;
}

ast_type_t* ast_type_array_size_unresolved(ast_type_t* element_type, ast_expr_t* size_expr)
{
    ast_type_t* tmp_array = malloc(sizeof(*tmp_array));

    *tmp_array = (ast_type_t){
        .kind = AST_TYPE_ARRAY,
        .data.array.element_type = element_type,
        .data.array.size_known = false,
        .data.array.size_expr = size_expr,
    };

    vec_push(gc_array, tmp_array);
    return tmp_array;
}

ast_type_t* ast_type_heap_array(ast_type_t* element_type)
{
    ast_type_t* array = hash_table_find(heap_array_cache, ssprintf("%p", element_type));
    if (array != nullptr)
        return array;

    array = malloc(sizeof(*array));

    *array = (ast_type_t){
        .kind = AST_TYPE_HEAP_ARRAY,
        .data.heap_array.element_type = element_type,
    };

    hash_table_insert(heap_array_cache, ssprintf("%p", element_type), array);
    return array;
}

ast_type_t* ast_type_view(ast_type_t* element_type)
{
    ast_type_t* view = hash_table_find(view_cache, ssprintf("%p", element_type));
    if (view != nullptr)
        return view;

    view = malloc(sizeof(*view));

    *view = (ast_type_t){
        .kind = AST_TYPE_VIEW,
        .data.view.element_type = element_type,
    };

    hash_table_insert(view_cache, ssprintf("%p", element_type), view);
    return view;
}

ast_type_t* ast_type_invalid()
{
    return invalid_cache;
}

ast_type_t* ast_type_from_token(token_t* tok)
{
    // Resolve token -> type
    switch (tok->type)
    {
        case TOKEN_BOOL:  return ast_type_builtin(TYPE_BOOL);
        case TOKEN_VOID:  return ast_type_builtin(TYPE_VOID);
        case TOKEN_I8:    return ast_type_builtin(TYPE_I8);
        case TOKEN_I16:   return ast_type_builtin(TYPE_I16);
        case TOKEN_I32:   return ast_type_builtin(TYPE_I32);
        case TOKEN_I64:   return ast_type_builtin(TYPE_I64);
        case TOKEN_ISIZE: return ast_type_builtin(TYPE_ISIZE);
        case TOKEN_U8:    return ast_type_builtin(TYPE_U8);
        case TOKEN_U16:   return ast_type_builtin(TYPE_U16);
        case TOKEN_U32:   return ast_type_builtin(TYPE_U32);
        case TOKEN_U64:   return ast_type_builtin(TYPE_U64);
        case TOKEN_USIZE: return ast_type_builtin(TYPE_USIZE);
        case TOKEN_F32:   return ast_type_builtin(TYPE_F32);
        case TOKEN_F64:   return ast_type_builtin(TYPE_F64);
        case TOKEN_NULL:  return ast_type_builtin(TYPE_NULL);

        case TOKEN_IDENTIFIER:
        return ast_type_user(tok->value);

        default:
            return ast_type_invalid();
    }
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
        case TYPE_ISIZE:
        case TYPE_U8:
        case TYPE_U16:
        case TYPE_U32:
        case TYPE_U64:
        case TYPE_USIZE:
        case TYPE_F32:
        case TYPE_F64:
            return true;
        default:
            return false;
    }
}

bool ast_type_is_integer(ast_type_t* type)
{
    if (type->kind != AST_TYPE_BUILTIN)
        return false;

    switch (type->data.builtin.type)
    {
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
        case TYPE_I64:
        case TYPE_ISIZE:
        case TYPE_U8:
        case TYPE_U16:
        case TYPE_U32:
        case TYPE_U64:
        case TYPE_USIZE:
            return true;
        default:
            return false;
    }
}

bool ast_type_is_real(ast_type_t* type)
{
    if (type->kind != AST_TYPE_BUILTIN)
        return false;

    switch (type->data.builtin.type)
    {
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
        case TYPE_ISIZE:
            return true;
        default:
            return false;
    }
}

size_t ast_type_sizeof(ast_type_t* type)
{
    // FIXME: Currently assumes target architecture == host architecture
    // FIXME: Does not work for User defined types

    switch (type->kind)
    {
        case AST_TYPE_BUILTIN:
            switch (type->data.builtin.type)
            {
                case TYPE_BOOL: return 1;
                case TYPE_I8: return 1;
                case TYPE_I16: return 2;
                case TYPE_I32: return 4;
                case TYPE_I64: return 8;
                case TYPE_ISIZE: return sizeof(void*);
                case TYPE_U8: return 1;
                case TYPE_U16: return 2;
                case TYPE_U32: return 4;
                case TYPE_U64: return 8;
                case TYPE_USIZE: return sizeof(void*);
                case TYPE_F32: return 4;
                case TYPE_F64: return 8;
                case TYPE_VOID:
                case TYPE_NULL:
                case TYPE_UNINIT:
                case TYPE_END:
                    panic("undefined sizeof for %d", type->data.builtin.type);
            }
            break;
        case AST_TYPE_ARRAY:
            panic_if(!type->data.array.size_known);
            return ast_type_sizeof(type->data.array.element_type) * type->data.array.size;
        case AST_TYPE_VIEW:
            return 2 * sizeof(void*);
        case AST_TYPE_POINTER:
            return sizeof(void*);
        case AST_TYPE_HEAP_ARRAY:
        case AST_TYPE_USER:
        case AST_TYPE_INVALID:
            panic("Unhandled type kind %d", type->kind);
    }

    panic("Unreachable");
}

bool ast_type_has_equality(ast_type_t* type)
{
    if (type->kind == AST_TYPE_BUILTIN)
        return type->data.builtin.type != TYPE_VOID;
    else if (type->kind == AST_TYPE_POINTER)
        return true;
    return false;
}

bool ast_type_is_instantiable(ast_type_t* type)
{
    switch (type->kind)
    {
        case AST_TYPE_BUILTIN:
            return type->data.builtin.type != TYPE_VOID;
        case AST_TYPE_INVALID:
            return false;
        default:
            break;
    }
    return true;
}

ast_coercion_kind_t ast_type_can_coerce(ast_type_t* from, ast_type_t* to)
{
    if (from == to)
        return COERCION_EQUAL;

    // From null to pointer type is considered "equal"
    if (from->kind == AST_TYPE_BUILTIN && from->data.builtin.type == TYPE_NULL && to->kind == AST_TYPE_POINTER)
        return COERCION_EQUAL;

    // Array to View is always a valid implicit cast
    if (from->kind == AST_TYPE_ARRAY && to->kind == AST_TYPE_VIEW &&
        from->data.array.element_type == to->data.view.element_type)
        return COERCION_ALWAYS;

    // Heap Array to View is always a valid implicit cast
    if (from->kind == AST_TYPE_HEAP_ARRAY && to->kind == AST_TYPE_VIEW  &&
        from->data.heap_array.element_type == to->data.view.element_type)
        return COERCION_ALWAYS;

    // Uninit to Array
    if (from->kind == AST_TYPE_BUILTIN && from->data.builtin.type == TYPE_UNINIT && to->kind == AST_TYPE_ARRAY)
        return COERCION_ALWAYS;

    // Check integer specific coercions
    if (ast_type_is_integer(from) && ast_type_is_integer(to))
    {
        if (ast_type_is_signed(from) != ast_type_is_signed(to))
            return COERCION_SIGNEDNESS;

        int from_sz = ast_type_sizeof(from);
        int to_sz = ast_type_sizeof(to);
        if (from_sz == to_sz)
            return COERCION_EQUAL;
        if (from_sz < to_sz)
            return COERCION_WIDEN;
    }

    // Check floating point specific coercions
    if (ast_type_is_real(from) && ast_type_is_real(to))
    {

    }

    return COERCION_INVALID;
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
        {
            if (type->data.pointer.str_repr == nullptr)
            {
                string_t tmp = STRING_INIT;
                string_append_cstr(&tmp, ast_type_string(type->data.pointer.pointee));
                string_append_char(&tmp, '*');
                type->data.pointer.str_repr = string_release(&tmp);
            }
            return type->data.pointer.str_repr;
        }
        case AST_TYPE_ARRAY:
        {
            if (type->data.array.str_repr == nullptr)
            {
                string_t tmp = STRING_INIT;
                string_append_char(&tmp, '[');
                string_append_cstr(&tmp, ast_type_string(type->data.array.element_type));
                string_append_cstr(&tmp, ", ");
                if (type->data.array.size_known)
                    string_append_cstr(&tmp, ssprintf("%lld", (long long)type->data.array.size));
                else
                    string_append_cstr(&tmp, " <expr>");
                string_append_char(&tmp, ']');
                type->data.array.str_repr = string_release(&tmp);
            }
            return type->data.array.str_repr;
        }
        case AST_TYPE_HEAP_ARRAY:
        {
            if (type->data.heap_array.str_repr == nullptr)
            {
                string_t tmp = STRING_INIT;
                string_append_char(&tmp, '[');
                string_append_cstr(&tmp, ast_type_string(type->data.heap_array.element_type));
                string_append_char(&tmp, ']');
                type->data.heap_array.str_repr = string_release(&tmp);
            }
            return type->data.heap_array.str_repr;
        }
        case AST_TYPE_VIEW:
        {
            if (type->data.view.str_repr == nullptr)
            {
                string_t tmp = STRING_INIT;
                string_append_cstr(&tmp, "view[");
                string_append_cstr(&tmp, ast_type_string(type->data.view.element_type));
                string_append_char(&tmp, ']');
                type->data.view.str_repr = string_release(&tmp);
            }
            return type->data.view.str_repr;
        }
    }

    panic("Case %d not handled", type->kind);
}

const char* type_to_str(type_t type)
{
    switch (type)
    {
        case TYPE_UNINIT: return "uninit";
        case TYPE_VOID:   return "void";
        case TYPE_BOOL:   return "bool";
        case TYPE_I8:     return "i8";
        case TYPE_I16:    return "i16";
        case TYPE_I32:    return "i32";
        case TYPE_I64:    return "i64";
        case TYPE_ISIZE:  return "isize";
        case TYPE_U8:     return "u8";
        case TYPE_U16:    return "u16";
        case TYPE_U32:    return "u32";
        case TYPE_U64:    return "u64";
        case TYPE_USIZE:  return "usize";
        case TYPE_F32:    return "f32";
        case TYPE_F64:    return "f64";
        case TYPE_NULL:   return "null_t";
        case TYPE_END:    panic("Not a valid value");
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

    user_cache = hash_table_create(ast_type_destroy);
    pointer_cache = hash_table_create(ast_type_destroy);
    fixed_array_cache = hash_table_create(ast_type_destroy);
    heap_array_cache = hash_table_create(ast_type_destroy);
    view_cache = hash_table_create(ast_type_destroy);
    gc_array = vec_create(ast_type_destroy);
}

__attribute__((destructor))
void ast_type_cache_cleanup()
{
    free(invalid_cache);
    for (int type = 0; type < TYPE_END; ++type)
        free(builtins_cache[type]);
    hash_table_destroy(user_cache);
    hash_table_destroy(pointer_cache);
    hash_table_destroy(fixed_array_cache);
    hash_table_destroy(heap_array_cache);
    hash_table_destroy(view_cache);
    vec_destroy(gc_array);
}
