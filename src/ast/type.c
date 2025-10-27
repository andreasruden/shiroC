#include "type.h"

#include "ast/expr/expr.h"
#include "ast/node.h"
#include "common/containers/hash_table.h"
#include "common/containers/string.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "common/util/ssprintf.h"
#include "parser/lexer.h"
#include "sema/symbol.h"

#include <string.h>

// Managed by ast_type_cache_init() and ast_type_cache_cleanup()
ast_type_t* invalid_cache = nullptr;
static ast_type_t* builtins_cache[TYPE_END] = {};
static hash_table_t* user_cache = nullptr;
static hash_table_t* user_unresolved_cache = nullptr;
static hash_table_t* pointer_cache = nullptr;  // Key: pointee type address -> Value: pointer type
static hash_table_t* fixed_array_cache = nullptr;  // Key: element type address, size -> Value: array type
static hash_table_t* heap_array_cache = nullptr;  // Key: element type address -> Value: array type
static hash_table_t* view_cache = nullptr;  // Key: element type address -> Value: view type
static hash_table_t* type_variable_cache = nullptr;  // Key: variable name -> Value: type variable
static hash_table_t* template_instance_cache = nullptr;  // Key: generated key -> Value: template instance
static vec_t* gc_array = nullptr; // unresolved fixed size arrays for garbage collection

static void set_default_traits(ast_type_t* type);

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
    {
        // NOTE: class_symbol is owned by semantic_context, not by the type
        free(type->data.user.name);
    }
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
    else if (type->kind == AST_TYPE_VARIABLE)
        free(type->data.type_variable.name);
    else if (type->kind == AST_TYPE_TEMPLATE_INSTANCE)
    {
        free(type->data.template_instance.type_arguments);
        free(type->data.template_instance.str_repr);
    }

    free(type);
}

ast_type_t* ast_type_builtin(type_t type)
{
    ast_type_t* ast_type = builtins_cache[type];
    panic_if(ast_type == nullptr);
    return ast_type;
}

ast_type_t* ast_type_user(symbol_t* class_symbol)
{
    ast_type_t* ast_type = hash_table_find(user_cache, class_symbol->fully_qualified_name);
    if (ast_type != nullptr)
        return ast_type;

    ast_type = malloc(sizeof(ast_type_t));
    panic_if(ast_type == nullptr);

    *ast_type = (ast_type_t){
        .kind = AST_TYPE_USER,
        .data.user.class_symbol = class_symbol,
    };
    set_default_traits(ast_type);
    hash_table_insert(user_cache, class_symbol->fully_qualified_name, ast_type);

    return ast_type;
}

ast_type_t* ast_type_user_unresolved(const char* type_name)
{
    ast_type_t* ast_type = hash_table_find(user_unresolved_cache, type_name);
    if (ast_type != nullptr)
        return ast_type;

    ast_type = malloc(sizeof(ast_type_t));
    panic_if(ast_type == nullptr);

    *ast_type = (ast_type_t){
        .kind = AST_TYPE_USER,
        .data.user.name = strdup(type_name),
        .data.user.type_arguments = nullptr,
        .data.user.num_type_arguments = 0,
    };
    set_default_traits(ast_type);
    hash_table_insert(user_unresolved_cache, type_name, ast_type);

    return ast_type;
}

ast_type_t* ast_type_user_unresolved_with_args(const char* type_name, ast_type_t** type_args, size_t num_type_args)
{
    // Generate cache key: type name + type argument addresses
    string_t key_str = STRING_INIT;
    string_append_cstr(&key_str, type_name);
    for (size_t i = 0; i < num_type_args; ++i)
        string_append_cstr(&key_str, ssprintf("<%p>", type_args[i]));
    const char* key = string_release(&key_str);

    ast_type_t* ast_type = hash_table_find(user_unresolved_cache, key);
    if (ast_type != nullptr)
        return ast_type;

    ast_type = malloc(sizeof(ast_type_t));
    panic_if(ast_type == nullptr);

    // Copy type arguments array
    ast_type_t** args_copy = malloc(sizeof(ast_type_t*) * num_type_args);
    memcpy(args_copy, type_args, sizeof(ast_type_t*) * num_type_args);

    *ast_type = (ast_type_t){
        .kind = AST_TYPE_USER,
        .data.user.name = strdup(type_name),
        .data.user.type_arguments = args_copy,
        .data.user.num_type_arguments = num_type_args,
    };
    set_default_traits(ast_type);
    hash_table_insert(user_unresolved_cache, key, ast_type);

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
    set_default_traits(pointer);
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
    set_default_traits(array);
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
    set_default_traits(tmp_array);
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
    set_default_traits(array);
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
    set_default_traits(view);
    hash_table_insert(view_cache, ssprintf("%p", element_type), view);
    return view;
}

ast_type_t* ast_type_invalid()
{
    return invalid_cache;
}

ast_type_t* ast_type_variable(const char* name)
{
    ast_type_t* type_var = hash_table_find(type_variable_cache, name);
    if (type_var != nullptr)
        return type_var;

    type_var = malloc(sizeof(*type_var));

    *type_var = (ast_type_t){
        .kind = AST_TYPE_VARIABLE,
        .data.type_variable.name = strdup(name),
    };
    set_default_traits(type_var);
    hash_table_insert(type_variable_cache, name, type_var);
    return type_var;
}

ast_type_t* ast_type_template_instance(symbol_t* template_symbol, ast_type_t** type_args, size_t num_type_args)
{
    // Generate cache key: template symbol address + type argument addresses
    string_t key_str = STRING_INIT;
    string_append_cstr(&key_str, ssprintf("%p", template_symbol));
    for (size_t i = 0; i < num_type_args; ++i)
        string_append_cstr(&key_str, ssprintf(",%p", type_args[i]));
    const char* key = string_release(&key_str);

    ast_type_t* instance = hash_table_find(template_instance_cache, key);
    if (instance != nullptr)
        return instance;

    instance = malloc(sizeof(*instance));

    // Copy type arguments array
    ast_type_t** args_copy = malloc(sizeof(ast_type_t*) * num_type_args);
    memcpy(args_copy, type_args, sizeof(ast_type_t*) * num_type_args);

    *instance = (ast_type_t){
        .kind = AST_TYPE_TEMPLATE_INSTANCE,
        .data.template_instance.template_symbol = template_symbol,
        .data.template_instance.type_arguments = args_copy,
        .data.template_instance.num_type_arguments = num_type_args,
    };
    set_default_traits(instance);
    hash_table_insert(template_instance_cache, key, instance);
    return instance;
}

void ast_type_set_trait(ast_type_t* type, ast_trait_t trait)
{
    type->traits[trait] = true;
}

void ast_type_clear_trait(ast_type_t* type, ast_trait_t trait)
{
    type->traits[trait] = false;
}

bool ast_type_has_trait(ast_type_t* type, ast_trait_t trait)
{
    return type->traits[trait];
}

ast_type_t* ast_type_from_token(token_t* tok)
{
    // Resolve token -> type
    switch (tok->type)
    {
        case TOKEN_BOOL:   return ast_type_builtin(TYPE_BOOL);
        case TOKEN_VOID:   return ast_type_builtin(TYPE_VOID);
        case TOKEN_I8:     return ast_type_builtin(TYPE_I8);
        case TOKEN_I16:    return ast_type_builtin(TYPE_I16);
        case TOKEN_I32:    return ast_type_builtin(TYPE_I32);
        case TOKEN_I64:    return ast_type_builtin(TYPE_I64);
        case TOKEN_ISIZE:  return ast_type_builtin(TYPE_ISIZE);
        case TOKEN_U8:     return ast_type_builtin(TYPE_U8);
        case TOKEN_U16:    return ast_type_builtin(TYPE_U16);
        case TOKEN_U32:    return ast_type_builtin(TYPE_U32);
        case TOKEN_U64:    return ast_type_builtin(TYPE_U64);
        case TOKEN_USIZE:  return ast_type_builtin(TYPE_USIZE);
        case TOKEN_F32:    return ast_type_builtin(TYPE_F32);
        case TOKEN_F64:    return ast_type_builtin(TYPE_F64);
        case TOKEN_STRING: return ast_type_builtin(TYPE_STRING);
        case TOKEN_NULL:   return ast_type_builtin(TYPE_NULL);

        case TOKEN_IDENTIFIER:
            return ast_type_user_unresolved(tok->value);

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
                case TYPE_STRING: return 2 * sizeof(void*);
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
        case AST_TYPE_VARIABLE:
        case AST_TYPE_TEMPLATE_INSTANCE:
            panic("Unhandled type kind %d", type->kind);
    }

    panic("Unreachable");
}

bool ast_type_has_equality(ast_type_t* type)
{
    if (type->kind == AST_TYPE_BUILTIN)
        return type->data.builtin.type != TYPE_VOID && type->data.builtin.type != TYPE_STRING;
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
        int from_sz = ast_type_sizeof(from);
        int to_sz = ast_type_sizeof(to);
        if (from_sz == to_sz)
            return COERCION_EQUAL;
        if (from_sz < to_sz)
            return COERCION_WIDEN;
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
        {
            if (type->data.user.class_symbol)
                return type->data.user.class_symbol->fully_qualified_name;
            return type->data.user.name;
        }
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
        case AST_TYPE_VARIABLE:
            return type->data.type_variable.name;
        case AST_TYPE_TEMPLATE_INSTANCE:
        {
            if (type->data.template_instance.str_repr == nullptr)
            {
                string_t tmp = STRING_INIT;
                string_append_cstr(&tmp, type->data.template_instance.template_symbol->name);
                string_append_char(&tmp, '<');
                for (size_t i = 0; i < type->data.template_instance.num_type_arguments; ++i)
                {
                    string_append_cstr(&tmp, ast_type_string(type->data.template_instance.type_arguments[i]));
                    if (i + 1 < type->data.template_instance.num_type_arguments)
                        string_append_cstr(&tmp, ", ");
                }
                string_append_char(&tmp, '>');
                type->data.template_instance.str_repr = string_release(&tmp);
            }
            return type->data.template_instance.str_repr;
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
        case TYPE_STRING: return "string";
        case TYPE_END:    panic("Not a valid value");
    }

    panic("Case %d not handled", type);
}

static void set_default_traits(ast_type_t* type)
{
    // Default: everything is copyable
    type->traits[TRAIT_COPYABLE] = true;

    // Set traits based on type kind
    if (type->kind == AST_TYPE_BUILTIN)
    {
        type_t builtin_type = type->data.builtin.type;

        // Arithmetic types support arithmetic and comparison operations
        if (builtin_type == TYPE_I8 || builtin_type == TYPE_I16 || builtin_type == TYPE_I32 ||
            builtin_type == TYPE_I64 || builtin_type == TYPE_ISIZE ||
            builtin_type == TYPE_U8 || builtin_type == TYPE_U16 || builtin_type == TYPE_U32 ||
            builtin_type == TYPE_U64 || builtin_type == TYPE_USIZE ||
            builtin_type == TYPE_F32 || builtin_type == TYPE_F64)
        {
            type->traits[TRAIT_ARITHMETIC] = true;
            type->traits[TRAIT_COMPARABLE] = true;
        }
    }
    else if (type->kind == AST_TYPE_POINTER)
    {
        type->traits[TRAIT_DEREFERENCEABLE] = true;
        type->traits[TRAIT_SUBSCRIPTABLE] = true;
    }
    else if (type->kind == AST_TYPE_ARRAY || type->kind == AST_TYPE_HEAP_ARRAY || type->kind == AST_TYPE_VIEW)
    {
        type->traits[TRAIT_SUBSCRIPTABLE] = true;
    }
    else if (type->kind == AST_TYPE_VARIABLE)
    {
        // Type variables get all traits by default (will be constrained by trait bounds in the future)
        type->traits[TRAIT_ARITHMETIC] = true;
        type->traits[TRAIT_COMPARABLE] = true;
        type->traits[TRAIT_SUBSCRIPTABLE] = true;
        type->traits[TRAIT_DEREFERENCEABLE] = true;
    }
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
        set_default_traits(ast_type);
        builtins_cache[type] = ast_type;
    }

    user_cache = hash_table_create(ast_type_destroy);
    user_unresolved_cache = hash_table_create(ast_type_destroy);
    pointer_cache = hash_table_create(ast_type_destroy);
    fixed_array_cache = hash_table_create(ast_type_destroy);
    heap_array_cache = hash_table_create(ast_type_destroy);
    view_cache = hash_table_create(ast_type_destroy);
    type_variable_cache = hash_table_create(ast_type_destroy);
    template_instance_cache = hash_table_create(ast_type_destroy);
    gc_array = vec_create(ast_type_destroy);
}

void ast_type_cache_reset()
{
    // Clear all non-builtin type caches. This is needed between unit tests
    // to prevent cached types from holding dangling pointers to freed symbols.
    hash_table_destroy(user_cache);
    hash_table_destroy(user_unresolved_cache);
    hash_table_destroy(pointer_cache);
    hash_table_destroy(fixed_array_cache);
    hash_table_destroy(heap_array_cache);
    hash_table_destroy(view_cache);
    hash_table_destroy(type_variable_cache);
    hash_table_destroy(template_instance_cache);
    vec_destroy(gc_array);

    // Recreate the caches
    user_cache = hash_table_create(ast_type_destroy);
    user_unresolved_cache = hash_table_create(ast_type_destroy);
    pointer_cache = hash_table_create(ast_type_destroy);
    fixed_array_cache = hash_table_create(ast_type_destroy);
    heap_array_cache = hash_table_create(ast_type_destroy);
    view_cache = hash_table_create(ast_type_destroy);
    type_variable_cache = hash_table_create(ast_type_destroy);
    template_instance_cache = hash_table_create(ast_type_destroy);
    gc_array = vec_create(ast_type_destroy);
}

__attribute__((destructor))
void ast_type_cache_cleanup()
{
    free(invalid_cache);
    for (int type = 0; type < TYPE_END; ++type)
        free(builtins_cache[type]);
    hash_table_destroy(user_cache);
    hash_table_destroy(user_unresolved_cache);
    hash_table_destroy(pointer_cache);
    hash_table_destroy(fixed_array_cache);
    hash_table_destroy(heap_array_cache);
    hash_table_destroy(view_cache);
    vec_destroy(gc_array);
}
