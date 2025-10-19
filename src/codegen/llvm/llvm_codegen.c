#include "llvm_codegen.h"

#include "ast/decl/param_decl.h"
#include "ast/def/class_def.h"
#include "ast/def/method_def.h"
#include "ast/expr/coercion_expr.h"
#include "ast/expr/construct_expr.h"
#include "ast/expr/member_init.h"
#include "ast/expr/self_expr.h"
#include "ast/expr/uninit_lit.h"
#include "ast/node.h"
#include "ast/type.h"
#include "ast/util/presenter.h"
#include "ast/visitor.h"
#include "common/containers/hash_table.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "common/util/ssprintf.h"
#include "parser/lexer.h"
#include "llvm_type_utils.h"

#include <llvm-c/Types.h>
#include <llvm-c/Core.h>
#include <llvm-c/DebugInfo.h>
#include <stdlib.h>
#include <string.h>

typedef struct class_layout class_layout_t;

struct llvm_codegen
{
    ast_visitor_t base;

    LLVMContextRef context;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMValueRef current_function;

    ast_presenter_t* presenter;
    hash_table_t* symbols;  // name (char*) -> LLVMValueRef (alloca)

    // Classes
    hash_table_t class_layouts;    // class name (char*) -> class_layout_t*
    ast_class_def_t* current_class; // set during method generation

    // Debug info
    LLVMDIBuilderRef di_builder;
    LLVMMetadataRef di_compile_unit;
    LLVMMetadataRef di_file;
    hash_table_t* di_scopes;  // function name (char*) -> LLVMMetadataRef (DISubprogram)
    LLVMMetadataRef current_di_scope;

    bool lvalue;
    bool address_of_lvalue;
    bool function_name;
};

struct class_layout
{
    char* class_name;
    hash_table_t member_indices;  // member name -> int (index in struct)
    vec_t member_names;  // member names (char*) in order they appear in LLVM struct
    vec_t member_types;  // member type (ast_type_t*) in order they appear in LLVM struct
};

// Get or declare llvm.ubsantrap intrinsic
static LLVMValueRef get_ubsantrap_intrinsic(llvm_codegen_t* llvm)
{
    const char* intrinsic_name = "llvm.ubsantrap";
    LLVMValueRef fn = LLVMGetNamedFunction(llvm->module, intrinsic_name);
    if (fn != nullptr)
        return fn;

    // declare void @llvm.ubsantrap(i8) noreturn nounwind
    LLVMTypeRef param_types[] = { LLVMInt8TypeInContext(llvm->context) };
    LLVMTypeRef fn_type = LLVMFunctionType(LLVMVoidTypeInContext(llvm->context), param_types, 1, false);
    return LLVMAddFunction(llvm->module, intrinsic_name, fn_type);
}

// Emit a bounds check failure trap
static void emit_bounds_check_trap(llvm_codegen_t* llvm, LLVMValueRef condition, const char* safe_label)
{
    LLVMBasicBlockRef trap_block = LLVMAppendBasicBlock(llvm->current_function, "bounds_check.trap");
    LLVMBasicBlockRef safe_block = LLVMAppendBasicBlock(llvm->current_function, safe_label);

    LLVMBuildCondBr(llvm->builder, condition, safe_block, trap_block);

    // Emit trap block
    LLVMPositionBuilderAtEnd(llvm->builder, trap_block);
    LLVMValueRef ubsantrap = get_ubsantrap_intrinsic(llvm);
    LLVMValueRef trap_kind = LLVMConstInt(LLVMInt8TypeInContext(llvm->context), 5, false);  // 5 = out-of-bounds
    LLVMBuildCall2(llvm->builder, LLVMGlobalGetValueType(ubsantrap), ubsantrap, &trap_kind, 1, "");
    LLVMBuildUnreachable(llvm->builder);

    // Continue with safe block
    LLVMPositionBuilderAtEnd(llvm->builder, safe_block);
}

// Register builtin functions that are implemented in the runtime
static void register_builtins(llvm_codegen_t* llvm)
{
    // printI32(i32) -> void
    LLVMTypeRef print_i32_param_types[] = { LLVMInt32TypeInContext(llvm->context) };
    LLVMTypeRef print_i32_type = LLVMFunctionType(LLVMVoidTypeInContext(llvm->context), print_i32_param_types, 1,
        false);
    LLVMAddFunction(llvm->module, "printI32", print_i32_type);
}

// Helper function to set debug location for the next instruction(s)
static void set_debug_location(llvm_codegen_t* llvm, ast_node_t* node)
{
    if (llvm->di_builder == nullptr || llvm->current_di_scope == nullptr)
        return;

    LLVMMetadataRef debug_loc = LLVMDIBuilderCreateDebugLocation(llvm->context, (unsigned int)node->source_begin.line,
        (unsigned int)node->source_begin.column, llvm->current_di_scope, nullptr);
    LLVMSetCurrentDebugLocation2(llvm->builder, debug_loc);
}

static void emit_root(void* self_, ast_root_t* root, void* out_)
{
    llvm_codegen_t* llvm = self_;
    (void)out_;

    for (size_t i = 0; i < vec_size(&root->tl_defs); ++i)
        ast_visitor_visit(llvm, vec_get(&root->tl_defs, i), nullptr);
}

static void emit_param_decl(void* self_, ast_param_decl_t* param, void* out_)
{
    (void)param;
    llvm_codegen_t* llvm = self_;
    LLVMValueRef original_param_ref = out_;  // out_ is actually in here...

    LLVMValueRef alloc_ref = LLVMBuildAlloca(llvm->builder, llvm_type(llvm->context, param->type),
        ssprintf("%s.addr", param->name));
    LLVMBuildStore(llvm->builder, original_param_ref, alloc_ref);

    hash_table_insert(llvm->symbols, param->name, alloc_ref);
}

static void emit_var_decl(void* self_, ast_var_decl_t* var, void* out_)
{
    llvm_codegen_t* llvm = self_;
    LLVMValueRef* out_ref = out_;

    set_debug_location(llvm, AST_NODE(var));
    LLVMValueRef alloc_ref = LLVMBuildAlloca(llvm->builder, llvm_type(llvm->context, var->type), var->name);

    if (var->init_expr != nullptr)
    {
        LLVMValueRef init_value = nullptr;
        ast_visitor_visit(llvm, var->init_expr, &init_value);
        // If RHS is "uninit", init_value will be nullptr
        if (init_value != nullptr)
        {
            if (var->type->kind == AST_TYPE_ARRAY)
            {
                LLVMTypeRef array_type = llvm_type(llvm->context, var->type);
                LLVMValueRef array_val = LLVMBuildLoad2(llvm->builder, array_type, init_value, "load_array");
                LLVMBuildStore(llvm->builder, array_val, alloc_ref);
            }
            else if (var->type->kind == AST_TYPE_USER)
            {
                LLVMTypeRef class_type = llvm_type(llvm->context, var->type);
                LLVMValueRef class_val = LLVMBuildLoad2(llvm->builder, class_type, init_value, "load_class");
                LLVMBuildStore(llvm->builder, class_val, alloc_ref);
            }
            else
                LLVMBuildStore(llvm->builder, init_value, alloc_ref);
        }
    }

    hash_table_insert(llvm->symbols, var->name, alloc_ref);
    *out_ref = alloc_ref;
}

static void emit_class_def(void* self_, ast_class_def_t* class_def, void* out_)
{
    (void)out_;
    llvm_codegen_t* llvm = self_;

    // Register class by name
    LLVMTypeRef class_type = LLVMStructCreateNamed(llvm->context, class_def->base.name);

    // Register the fields of the class
    unsigned int member_count = vec_size(&class_def->members);
    LLVMTypeRef* member_types = malloc(sizeof(LLVMTypeRef) * member_count);
    for (unsigned int i = 0; i < member_count; ++i)
    {
        ast_member_decl_t* member = vec_get(&class_def->members, i);
        LLVMTypeRef member_type = llvm_type(llvm->context, member->base.type);
        member_types[i] = member_type;
    }
    LLVMStructSetBody(class_type, member_types, member_count, false);
    free(member_types);

    // Create class layout
    class_layout_t* layout = malloc(sizeof(*layout));
    *layout = (class_layout_t){
        .class_name = strdup(class_def->base.name),
        .member_names = VEC_INIT(free),
        .member_types = VEC_INIT(nullptr),
        .member_indices = HASH_TABLE_INIT(nullptr),
    };

    // Populate class layout
    for (size_t i = 0; i < member_count; ++i)
    {
        ast_member_decl_t* decl = vec_get(&class_def->members, i);
        hash_table_insert(&layout->member_indices, decl->base.name, (void*)(intptr_t)i);
        vec_push(&layout->member_names, strdup(decl->base.name));
        vec_push(&layout->member_types, decl->base.type);
    }

    hash_table_insert(&llvm->class_layouts, class_def->base.name, layout);

    // Visit all methods
    llvm->current_class = class_def;
    for (size_t i = 0; i < vec_size(&class_def->methods); ++i)
        ast_visitor_visit(llvm, vec_get(&class_def->methods, i), nullptr);
    llvm->current_class = nullptr;
}

static void emit_fn_def(void* self_, ast_fn_def_t* fn_def, void* out_)
{
    llvm_codegen_t* llvm = self_;
    (void)out_;

    llvm->symbols = hash_table_create(nullptr);

    // Build list of params
    size_t param_count = vec_size(&fn_def->params);
    LLVMTypeRef* param_types = malloc(param_count * sizeof(LLVMTypeRef));
    for (size_t i = 0; i < param_count; ++i)
    {
        ast_param_decl_t* param = vec_get(&fn_def->params, i);
        param_types[i] = llvm_type(llvm->context, param->type);
    }

    // Emit function
    LLVMTypeRef fn_type = LLVMFunctionType(llvm_type(llvm->context, fn_def->return_type), param_types,
        param_count, false);
    LLVMValueRef fn_val = LLVMAddFunction(llvm->module, fn_def->base.name, fn_type);
    llvm->current_function = fn_val;
    free(param_types);

    // Create debug info for this function
    if (llvm->di_builder != nullptr)
    {
        // Create subroutine type (simplified - just mark as unspecified)
        LLVMMetadataRef di_param_types[] = {nullptr};
        LLVMMetadataRef di_fn_type = LLVMDIBuilderCreateSubroutineType(llvm->di_builder, llvm->di_file,
            di_param_types, 0, LLVMDIFlagZero);

        // Create subprogram (function debug info)
        LLVMMetadataRef di_subprogram = LLVMDIBuilderCreateFunction(llvm->di_builder, llvm->di_file, fn_def->base.name,
            strlen(fn_def->base.name), fn_def->base.name, strlen(fn_def->base.name), llvm->di_file,
            (unsigned int)AST_NODE(fn_def)->source_begin.line, di_fn_type, false, true,
            (unsigned int)AST_NODE(fn_def)->source_begin.line, LLVMDIFlagZero, false);

        // Attach subprogram to the function
        LLVMSetSubprogram(fn_val, di_subprogram);

        // Set as current scope for nested instructions
        llvm->current_di_scope = di_subprogram;
        hash_table_insert(llvm->di_scopes, fn_def->base.name, di_subprogram);
    }

    // Add entry block and position builder
    LLVMBasicBlockRef entry_block = LLVMAppendBasicBlock(fn_val, "entry");
    LLVMPositionBuilderAtEnd(llvm->builder, entry_block);

    // Set debug location for function entry
    set_debug_location(llvm, AST_NODE(fn_def));

    // Allocate space for all parameters
    for (size_t i = 0; i < vec_size(&fn_def->params); ++i)
        ast_visitor_visit(llvm, vec_get(&fn_def->params, i), LLVMGetParam(fn_val, i));

    LLVMValueRef out_val;
    ast_visitor_visit(llvm, fn_def->body, &out_val);

    if (fn_def->return_type == ast_type_builtin(TYPE_VOID))
        LLVMBuildRetVoid(llvm->builder);

    // Clear current scope when leaving function
    llvm->current_di_scope = nullptr;

    hash_table_destroy(llvm->symbols);
    llvm->symbols = nullptr;
}

static void emit_method_def(void* self_, ast_method_def_t* method, void* out_)
{
    llvm_codegen_t* llvm = self_;
    (void)out_;
    panic_if(llvm->current_class == nullptr);

    llvm->symbols = hash_table_create(nullptr);

    // Mangle method name: ClassName.methodName
    char* mangled_name = ssprintf("%s.%s", llvm->current_class->base.name, method->base.base.name);

    LLVMTypeRef class_ptr_type = LLVMPointerTypeInContext(llvm->context, 0);

    // Build list of params: [*ClassName, ...user_params]
    size_t user_param_count = vec_size(&method->base.params);
    size_t total_param_count = 1 + user_param_count;
    LLVMTypeRef* param_types = malloc(total_param_count * sizeof(LLVMTypeRef));
    param_types[0] = class_ptr_type;  // implicit self parameter
    for (size_t i = 0; i < user_param_count; ++i)
    {
        ast_param_decl_t* param = vec_get(&method->base.params, i);
        param_types[i + 1] = llvm_type(llvm->context, param->type);
    }

    // Emit function
    LLVMTypeRef fn_type = LLVMFunctionType(llvm_type(llvm->context, method->base.return_type), param_types,
        total_param_count, false);
    LLVMValueRef fn_val = LLVMAddFunction(llvm->module, mangled_name, fn_type);
    llvm->current_function = fn_val;
    free(param_types);

    // Create debug info for this method
    if (llvm->di_builder != nullptr)
    {
        // Create subroutine type (simplified - just mark as unspecified)
        LLVMMetadataRef di_param_types[] = {nullptr};
        LLVMMetadataRef di_fn_type = LLVMDIBuilderCreateSubroutineType(llvm->di_builder, llvm->di_file,
            di_param_types, 0, LLVMDIFlagZero);

        // Create subprogram (method debug info)
        char* debug_name = ssprintf("%s.%s", llvm->current_class->base.name, method->base.base.name);
        LLVMMetadataRef di_subprogram = LLVMDIBuilderCreateFunction(llvm->di_builder, llvm->di_file, debug_name,
            strlen(debug_name), debug_name, strlen(debug_name), llvm->di_file,
            (unsigned int)AST_NODE(method)->source_begin.line, di_fn_type, false, true,
            (unsigned int)AST_NODE(method)->source_begin.line, LLVMDIFlagZero, false);

        LLVMSetSubprogram(fn_val, di_subprogram);

        // Set as current scope for nested instructions
        llvm->current_di_scope = di_subprogram;
        hash_table_insert(llvm->di_scopes, debug_name, di_subprogram);
    }

    // Add entry block and position builder
    LLVMBasicBlockRef entry_block = LLVMAppendBasicBlock(fn_val, "entry");
    LLVMPositionBuilderAtEnd(llvm->builder, entry_block);

    set_debug_location(llvm, AST_NODE(method));

    // Allocate space for implicit 'self' parameter
    LLVMValueRef self_param = LLVMGetParam(fn_val, 0);
    LLVMValueRef self_alloc = LLVMBuildAlloca(llvm->builder, class_ptr_type, "self.addr");
    LLVMBuildStore(llvm->builder, self_param, self_alloc);
    hash_table_insert(llvm->symbols, "self", self_alloc);

    // Allocate space for all user parameters
    for (size_t i = 0; i < user_param_count; ++i)
        ast_visitor_visit(llvm, vec_get(&method->base.params, i), LLVMGetParam(fn_val, i + 1));

    LLVMValueRef out_val;
    ast_visitor_visit(llvm, method->base.body, &out_val);

    if (method->base.return_type == ast_type_builtin(TYPE_VOID))
        LLVMBuildRetVoid(llvm->builder);

    llvm->current_di_scope = nullptr;

    hash_table_destroy(llvm->symbols);
    llvm->symbols = nullptr;
}

static void emit_bool_lit(void* self_, ast_bool_lit_t* lit, void* out_)
{
    llvm_codegen_t* llvm = self_;
    LLVMValueRef* out_val = out_;

    *out_val = LLVMConstInt(llvm_type(llvm->context, lit->base.type), lit->value ? 1 : 0, false);
}

static void emit_float_lit(void* self_, ast_float_lit_t* lit, void* out_)
{
    llvm_codegen_t* llvm = self_;
    LLVMValueRef* out_val = out_;

    *out_val = LLVMConstReal(llvm_type(llvm->context, lit->base.type), lit->value);
}

static void emit_int_lit(void* self_, ast_int_lit_t* lit, void* out_)
{
    llvm_codegen_t* llvm = self_;
    LLVMValueRef* out_val = out_;

    *out_val = LLVMConstInt(llvm_type(llvm->context, lit->base.type), lit->value.as_unsigned,
        ast_type_is_signed(lit->base.type));
}

static void emit_member_access(void* self_, ast_member_access_t* access, void* out_)
{
    llvm_codegen_t* llvm = self_;
    LLVMValueRef* out_val = out_;

    // Get the class type (handle both direct class and pointer to class)
    ast_type_t* instance_type = access->instance->type;
    ast_type_t* class_type = instance_type;
    if (instance_type->kind == AST_TYPE_POINTER)
        class_type = instance_type->data.pointer.pointee;

    panic_if(class_type->kind != AST_TYPE_USER);
    const char* class_name = class_type->data.user.name;

    // Get the instance as an lvalue (address of the variable/expression)
    bool was_lvalue = llvm->lvalue;
    llvm->lvalue = true;
    LLVMValueRef instance_addr = nullptr;
    ast_visitor_visit(llvm, access->instance, &instance_addr);
    llvm->lvalue = was_lvalue;

    // If the instance type is a pointer, load it to get the pointer to the class
    // If the instance type is a value, instance_addr already points to the class
    LLVMValueRef instance_ptr = instance_addr;
    if (instance_type->kind == AST_TYPE_POINTER)
        instance_ptr = LLVMBuildLoad2(llvm->builder, LLVMPointerTypeInContext(llvm->context, 0), instance_addr, "");

    class_layout_t* layout = hash_table_find(&llvm->class_layouts, class_name);
    panic_if(layout == nullptr);
    panic_if(!hash_table_contains(&layout->member_indices, access->member_name));
    intptr_t index = (intptr_t)hash_table_find(&layout->member_indices, access->member_name);

    LLVMValueRef ptr_to_member = LLVMBuildStructGEP2(llvm->builder, llvm_type(llvm->context, class_type),
        instance_ptr, index, class_name);

    if (llvm->lvalue)
        *out_val = ptr_to_member;
    else
    {
        LLVMTypeRef member_type = llvm_type(llvm->context, (ast_type_t*)vec_get(&layout->member_types, (size_t)index));
        *out_val = LLVMBuildLoad2(llvm->builder, member_type, ptr_to_member, "member_val");
    }
}

static void emit_member_init(void* self_, ast_member_init_t* init, void* out_)
{
    llvm_codegen_t* llvm = self_;

    bool was_lvalue = llvm->lvalue;
    llvm->lvalue = false;
    ast_visitor_visit(self_, init->init_expr, out_);
    llvm->lvalue = was_lvalue;
}

static void emit_method_call(void* self_, ast_method_call_t* call, void* out_)
{
    llvm_codegen_t* llvm = self_;
    LLVMValueRef* out_val = out_;

    // Get the class type (handle both direct class and pointer to class)
    ast_type_t* instance_type = call->instance->type;
    ast_type_t* class_type = instance_type;
    if (instance_type->kind == AST_TYPE_POINTER)
        class_type = instance_type->data.pointer.pointee;

    panic_if(class_type->kind != AST_TYPE_USER);
    const char* class_name = class_type->data.user.name;

    set_debug_location(llvm, AST_NODE(call));

    // Mangle method name: ClassName.methodName
    char* mangled_name = ssprintf("%s.%s", class_name, call->method_name);

    // Get the instance as an lvalue (address of the variable/expression)
    bool was_lvalue = llvm->lvalue;
    llvm->lvalue = true;
    LLVMValueRef instance_addr = nullptr;
    ast_visitor_visit(llvm, call->instance, &instance_addr);
    llvm->lvalue = was_lvalue;
    panic_if(instance_addr == nullptr);

    // If the instance type is a pointer, load it to get the pointer to pass as self
    // If the instance type is a value, instance_addr is already the pointer to pass as self
    LLVMValueRef self = instance_addr;
    if (instance_type->kind == AST_TYPE_POINTER)
        self = LLVMBuildLoad2(llvm->builder, LLVMPointerTypeInContext(llvm->context, 0), instance_addr, "");

    // Emit all arguments
    size_t user_arg_count = vec_size(&call->arguments);
    size_t total_arg_count = user_arg_count + 1;
    LLVMValueRef* args = malloc(sizeof(LLVMValueRef) * total_arg_count);
    args[0] = self;  // Implicit self
    for (size_t i = 0; i < user_arg_count; ++i)
    {
        LLVMValueRef arg_val = nullptr;
        ast_visitor_visit(llvm, vec_get(&call->arguments, i), &arg_val);
        panic_if(arg_val == nullptr);
        args[i + 1] = arg_val;
    }

    // Emit call
    LLVMValueRef fn = LLVMGetNamedFunction(llvm->module, mangled_name);
    LLVMTypeRef fn_type = LLVMGlobalGetValueType(fn);
    LLVMValueRef call_result = LLVMBuildCall2(llvm->builder, fn_type, fn, args, (unsigned int)total_arg_count,
        call->base.type == ast_type_builtin(TYPE_VOID) ? "" : "method_call");

    if (out_val != nullptr)
        *out_val = call_result;

    if (args != nullptr)
        free(args);
}

static void emit_null_lit(void* self_, ast_null_lit_t* lit, void* out_)
{
    llvm_codegen_t* llvm = self_;
    LLVMValueRef* out_val = out_;
    (void)lit;

    *out_val = LLVMConstNull(LLVMPointerTypeInContext(llvm->context, 0));
}

static void emit_simple_assignment(void* self_, ast_bin_op_t* bin_op, void* out_)
{
    llvm_codegen_t* llvm = self_;
    (void)out_;

    LLVMValueRef rhs_value = nullptr;
    ast_visitor_visit(llvm, bin_op->rhs, &rhs_value);

    llvm->lvalue = true;
    LLVMValueRef lhs_addr = nullptr;
    ast_visitor_visit(llvm, bin_op->lhs, &lhs_addr);
    llvm->lvalue = false;

    LLVMBuildStore(llvm->builder, rhs_value, lhs_addr);
}

static void emit_other_bin_op(void* self_, ast_bin_op_t* bin_op, void* out_)
{
    llvm_codegen_t* llvm = self_;
    LLVMValueRef* out_val = out_;

    set_debug_location(llvm, AST_NODE(bin_op));

    LLVMValueRef lhs_value = nullptr;
    ast_visitor_visit(llvm, bin_op->lhs, &lhs_value);

    LLVMValueRef rhs_value = nullptr;
    ast_visitor_visit(llvm, bin_op->rhs, &rhs_value);

    LLVMValueRef result = nullptr;
    token_type_t op = bin_op->op;

    switch (op)
    {
        case TOKEN_PLUS_ASSIGN:
        case TOKEN_PLUS:
            result = LLVMBuildAdd(llvm->builder, lhs_value, rhs_value, "add");
            break;
        case TOKEN_MINUS_ASSIGN:
        case TOKEN_MINUS:
            result = LLVMBuildSub(llvm->builder, lhs_value, rhs_value, "sub");
            break;
        case TOKEN_MUL_ASSIGN:
        case TOKEN_STAR:
            result = LLVMBuildMul(llvm->builder, lhs_value, rhs_value, "mul");
            break;
        case TOKEN_DIV_ASSIGN:
        case TOKEN_DIV:
            result = LLVMBuildSDiv(llvm->builder, lhs_value, rhs_value, "div");
            break;
        case TOKEN_MODULO_ASSIGN:
        case TOKEN_MODULO:
            result = LLVMBuildSRem(llvm->builder, lhs_value, rhs_value, "rem");
            break;
        case TOKEN_LT:
            result = LLVMBuildICmp(llvm->builder, LLVMIntSLT, lhs_value, rhs_value, "lt");
            break;
        case TOKEN_LTE:
            result = LLVMBuildICmp(llvm->builder, LLVMIntSLE, lhs_value, rhs_value, "lte");
            break;
        case TOKEN_GT:
            result = LLVMBuildICmp(llvm->builder, LLVMIntSGT, lhs_value, rhs_value, "gt");
            break;
        case TOKEN_GTE:
            result = LLVMBuildICmp(llvm->builder, LLVMIntSGE, lhs_value, rhs_value, "gte");
            break;
        case TOKEN_EQ:
            result = LLVMBuildICmp(llvm->builder, LLVMIntEQ, lhs_value, rhs_value, "eq");
            break;
        case TOKEN_NEQ:
            result = LLVMBuildICmp(llvm->builder, LLVMIntNE, lhs_value, rhs_value, "ne");
            break;
        default:
            panic("Unhandled binary op: %d", op);
    }

    if (token_type_is_assignment_op(op))
    {
        llvm->lvalue = true;
        LLVMValueRef lhs_addr = nullptr;
        ast_visitor_visit(llvm, bin_op->lhs, &lhs_addr);
        llvm->lvalue = false;
        LLVMBuildStore(llvm->builder, result, lhs_addr);
    }

    if (out_val != nullptr)
        *out_val = result;
}

static void emit_array_lit(void* self_, ast_array_lit_t* lit, void* out_)
{
    llvm_codegen_t* llvm = self_;
    LLVMValueRef* out_val = out_;

    LLVMTypeRef array_type = llvm_type(llvm->context, lit->base.type);
    LLVMValueRef array_alloc = LLVMBuildAlloca(llvm->builder, array_type, "array_lit");

    // Get the element type of the array
    ast_type_t* elem_type = lit->base.type->data.array.element_type;
    LLVMTypeRef llvm_elem_type = llvm_type(llvm->context, elem_type);

    for (size_t i = 0; i < vec_size(&lit->exprs); ++i)
    {
        LLVMValueRef elem_val = nullptr;
        ast_visitor_visit(llvm, vec_get(&lit->exprs, i), &elem_val);
        LLVMValueRef indices[] = {
            LLVMConstInt(LLVMInt64TypeInContext(llvm->context), 0, false),
            LLVMConstInt(LLVMInt64TypeInContext(llvm->context), (uint64_t)i, false)
        };
        LLVMValueRef elem_dst = LLVMBuildInBoundsGEP2(llvm->builder, array_type, array_alloc,  indices, 2, "elem_ptr");

        // If the element is itself an array (nested array literal), we need to copy the value
        if (elem_type->kind == AST_TYPE_ARRAY)
        {
            // Load the array value from the temporary allocation and store it
            LLVMValueRef array_val = LLVMBuildLoad2(llvm->builder, llvm_elem_type, elem_val, "load_array");
            LLVMBuildStore(llvm->builder, array_val, elem_dst);
        }
        else
        {
            // For non-array elements, just store directly
            LLVMBuildStore(llvm->builder, elem_val, elem_dst);
        }
    }

    if (out_val != nullptr)
        *out_val = array_alloc;
}

static LLVMValueRef emit_ptr_to_array_elem(llvm_codegen_t* llvm, ast_type_t* array_type, LLVMValueRef array_ptr,
    LLVMValueRef index)
{
    LLVMValueRef elem_ptr = nullptr;

    switch (array_type->kind)
    {
        case AST_TYPE_VIEW:
        {
            // For views: extract pointer from 2nd struct field and index into it
            LLVMTypeRef view_type = llvm_type(llvm->context, array_type);
            LLVMTypeRef elem_type = llvm_type(llvm->context, array_type->data.view.element_type);
            LLVMValueRef view_indices[] = {
                LLVMConstInt(LLVMInt32TypeInContext(llvm->context), 0, false),
                LLVMConstInt(LLVMInt32TypeInContext(llvm->context), 1, false),
            };
            LLVMValueRef ptr_to_field = LLVMBuildInBoundsGEP2(llvm->builder, view_type, array_ptr, view_indices, 2,
                "ptr_to_views_array_field");
            LLVMValueRef loaded_ptr = LLVMBuildLoad2(llvm->builder, LLVMPointerTypeInContext(llvm->context, 0),
                ptr_to_field, "array_ptr");
            elem_ptr = LLVMBuildInBoundsGEP2(llvm->builder, elem_type, loaded_ptr, &index, 1, "elem_ptr");
            break;
        }
        case AST_TYPE_ARRAY:
        {
            // For arrays: use two indices (dereference + index)
            // Note: semantic analyzer enforces that index is i32, so extend to i64 for GEP
            // FIXME: There should be a coercion node + type should be usize
            LLVMTypeRef fixed_array_type = llvm_type(llvm->context, array_type);
            LLVMValueRef index_i64 = LLVMBuildSExt(llvm->builder, index, LLVMInt64TypeInContext(llvm->context),
                "index_i64");
            LLVMValueRef indices[] = { LLVMConstInt(LLVMInt64TypeInContext(llvm->context), 0, false), index_i64 };
            elem_ptr = LLVMBuildInBoundsGEP2(llvm->builder, fixed_array_type, array_ptr, indices, 2, "elem_ptr");
            break;
        }
        case AST_TYPE_POINTER:
        {
            // For pointers: use the element type that the pointer points to
            LLVMTypeRef elem_type = llvm_type(llvm->context, array_type->data.pointer.pointee);
            elem_ptr = LLVMBuildGEP2(llvm->builder, elem_type, array_ptr, &index, 1, "elem_ptr");
            break;
        }
        default:
            panic("Unhandled type %s", ast_type_string(array_type));
    }

    return elem_ptr;
}

static LLVMValueRef emit_size_of_array(llvm_codegen_t* llvm, ast_type_t* array_type, LLVMValueRef array_ptr)
{
    switch (array_type->kind)
    {
        case AST_TYPE_ARRAY:
        {
            // Fixed-size arrays have compile-time known size
            return LLVMConstInt(LLVMInt64TypeInContext(llvm->context),
                (unsigned long long)array_type->data.array.size, false);
        }
        case AST_TYPE_VIEW:
        {
            // Views store their length in field 0 of the struct {i64 length, *element_type data}
            LLVMTypeRef view_type = llvm_type(llvm->context, array_type);
            LLVMTypeRef length_type = LLVMInt64TypeInContext(llvm->context);
            LLVMValueRef length_ptr = LLVMBuildStructGEP2(llvm->builder, view_type, array_ptr, 0, "length_ptr");
            return LLVMBuildLoad2(llvm->builder, length_type, length_ptr, "view_length");
        }
        default:
            panic("Unhandled type %s", ast_type_string(array_type));
    }
}

static LLVMValueRef build_view_struct(llvm_codegen_t* llvm, ast_type_t* view_type, LLVMValueRef length,
    LLVMValueRef first_elem_ptr)
{
    LLVMTypeRef llvm_view_type = llvm_type(llvm->context, view_type);
    LLVMValueRef view_struct = LLVMGetUndef(llvm_view_type);
    view_struct = LLVMBuildInsertValue(llvm->builder, view_struct, length, 0, "view_with_len");
    view_struct = LLVMBuildInsertValue(llvm->builder, view_struct, first_elem_ptr, 1, "view_with_data");
    return view_struct;
}

static void emit_array_slice(void* self_, ast_array_slice_t* slice, void* out_)
{
    llvm_codegen_t* llvm = self_;
    LLVMValueRef* out_val = out_;

    LLVMValueRef start = nullptr;  // inclusive
    LLVMValueRef end = nullptr;    // exclusive
    LLVMValueRef array_length = nullptr;  // only computed if needed for bounds checks

    // Get value of array
    bool output_lvalue = llvm->lvalue;
    llvm->lvalue = true;
    LLVMValueRef array_ptr = nullptr;
    ast_visitor_visit(llvm, slice->array, &array_ptr);
    llvm->lvalue = output_lvalue;

    // Can only compute array length for arrays and views, not raw pointers
    bool can_bounds_check = slice->array->type->kind != AST_TYPE_POINTER;

    // Compute array length if we'll need it for bounds checks or if end is not specified
    if (can_bounds_check && (!slice->bounds_safe || slice->end == nullptr))
        array_length = emit_size_of_array(llvm, slice->array->type, array_ptr);

    if (slice->start == nullptr)
        start = LLVMConstInt(LLVMInt64TypeInContext(llvm->context), 0, false);
    else
    {
        ast_visitor_visit(llvm, slice->start, &start);
        start = LLVMBuildZExt(llvm->builder, start, LLVMInt64TypeInContext(llvm->context), "start");
    }

    if (slice->end == nullptr)
    {
        // For raw pointers without explicit end, this is an error - sema should have caught this
        panic_if(!can_bounds_check);
        end = array_length;  // already computed above
    }
    else
    {
        ast_visitor_visit(llvm, slice->end, &end);
        end = LLVMBuildZExt(llvm->builder, end, LLVMInt64TypeInContext(llvm->context), "end");
    }

    // Emit bounds checks if not verified at compile-time
    // Note: we don't bounds-check raw pointer slices (only arrays and views)
    if (!slice->bounds_safe && can_bounds_check)
    {
        // Check: start <= end (ensures size >= 0)
        LLVMValueRef start_le_end = LLVMBuildICmp(llvm->builder, LLVMIntULE, start, end, "start_le_end");
        emit_bounds_check_trap(llvm, start_le_end, "slice.check_end");

        // Check: end <= array_length (only if end was explicitly provided)
        if (slice->end != nullptr)
        {
            LLVMValueRef end_le_len = LLVMBuildICmp(llvm->builder, LLVMIntULE, end, array_length, "end_le_len");
            emit_bounds_check_trap(llvm, end_le_len, "slice.safe");
        }
    }

    LLVMValueRef length = LLVMBuildSub(llvm->builder, end, start, "view_size");
    LLVMValueRef elem_ptr = emit_ptr_to_array_elem(llvm, slice->array->type, array_ptr, start);

    LLVMValueRef view = build_view_struct(llvm, slice->base.type, length, elem_ptr);
    if (out_val != nullptr)
        *out_val = view;
}

static void emit_array_subscript(void* self_, ast_array_subscript_t* subscript, void* out_)
{
    llvm_codegen_t* llvm = self_;
    LLVMValueRef* out_val = out_;

    set_debug_location(llvm, AST_NODE(subscript));

    ast_type_kind_t arr_kind = subscript->array->type->kind;
    panic_if(arr_kind != AST_TYPE_ARRAY && arr_kind != AST_TYPE_VIEW && arr_kind != AST_TYPE_POINTER);

    // Get value of array
    bool output_lvalue = llvm->lvalue;
    llvm->lvalue = true;
    LLVMValueRef array_ptr = nullptr;
    ast_visitor_visit(llvm, subscript->array, &array_ptr);
    llvm->lvalue = output_lvalue;

    // If the array expression is a reference to a pointer variable, load it
    if (subscript->array->type->kind == AST_TYPE_POINTER &&
        subscript->array->base.kind == AST_EXPR_REF)
    {
        // This is ptr_var[index] - load the pointer value
        LLVMTypeRef ptr_type = llvm_type(llvm->context, subscript->array->type);
        array_ptr = LLVMBuildLoad2(llvm->builder, ptr_type, array_ptr, "ptr_val");
    }

    // Get value of index (must be an rvalue, not an lvalue)
    llvm->lvalue = false;
    LLVMValueRef index = nullptr;
    ast_visitor_visit(llvm, subscript->index, &index);

    // Emit bounds check if not verified at compile-time
    // Note: we don't bounds-check raw pointer subscripts (only arrays and views)
    if (!subscript->bounds_safe && subscript->array->type->kind != AST_TYPE_POINTER)
    {
        LLVMValueRef array_length = emit_size_of_array(llvm, subscript->array->type, array_ptr);
        // Check: index < array_length (index is already usize/i64 from sema)
        LLVMValueRef in_bounds = LLVMBuildICmp(llvm->builder, LLVMIntULT, index, array_length, "in_bounds");
        emit_bounds_check_trap(llvm, in_bounds, "subscript.safe");
    }

    LLVMValueRef elem_ptr = emit_ptr_to_array_elem(llvm, subscript->array->type, array_ptr, index);

    if (output_lvalue)
    {
        if (out_val != nullptr)
            *out_val = elem_ptr;
    }
    else
    {
        LLVMValueRef element = LLVMBuildLoad2(llvm->builder, llvm_type(llvm->context, subscript->base.type), elem_ptr,
            "arr_elem");
        if (out_val != nullptr)
            *out_val = element;
    }
}

static void emit_bin_op(void* self_, ast_bin_op_t* bin_op, void* out_)
{
    if (bin_op->op == TOKEN_ASSIGN)
        emit_simple_assignment(self_, bin_op, out_);
    else
        emit_other_bin_op(self_, bin_op, out_);
}

static void emit_call_expr(void* self_, ast_call_expr_t* call, void* out_)
{
    llvm_codegen_t* llvm = self_;
    LLVMValueRef* out = out_;

    set_debug_location(llvm, AST_NODE(call));

    llvm->function_name = true;
    LLVMValueRef fn = nullptr;
    ast_visitor_visit(llvm, call->function, &fn);
    llvm->function_name = false;
    panic_if(fn == nullptr);

    size_t arg_count = vec_size(&call->arguments);
    LLVMValueRef* args = nullptr;
    if (arg_count > 0)
    {
        args = malloc(sizeof(LLVMValueRef) * arg_count);
        for (size_t i = 0; i < arg_count; ++i)
        {
            LLVMValueRef arg_val = nullptr;
            ast_visitor_visit(llvm, vec_get(&call->arguments, i), &arg_val);
            panic_if(arg_val == nullptr);
            args[i] = arg_val;
        }
    }

    LLVMTypeRef fn_type = LLVMGlobalGetValueType(fn);
    LLVMValueRef call_result = LLVMBuildCall2(llvm->builder, fn_type, fn, args, (unsigned int)arg_count,
        call->base.type == ast_type_builtin(TYPE_VOID) ? "" : "call");

    if (out != nullptr)
        *out = call_result;

    if (args != nullptr)
        free(args);
}

static void emit_coercion_expr(void* self_, ast_coercion_expr_t* coercion, void* out_)
{
    llvm_codegen_t* llvm = self_;
    LLVMValueRef* out_val = out_;

    ast_type_t* from_type = coercion->expr->type;
    ast_type_t* to_type = coercion->target;

    if (from_type->kind == AST_TYPE_ARRAY && to_type->kind == AST_TYPE_VIEW)
    {
        // Array to view coercion
        // Get the array value (as lvalue)
        bool output_lvalue = llvm->lvalue;
        llvm->lvalue = true;
        LLVMValueRef array_ptr = nullptr;
        ast_visitor_visit(llvm, coercion->expr, &array_ptr);
        llvm->lvalue = output_lvalue;

        LLVMValueRef length = LLVMConstInt(LLVMInt64TypeInContext(llvm->context),
            (unsigned long long)from_type->data.array.size, false);

        // Get pointer to first element
        LLVMValueRef indices[] = {
            LLVMConstInt(LLVMInt64TypeInContext(llvm->context), 0, false),
            LLVMConstInt(LLVMInt32TypeInContext(llvm->context), 0, false)
        };
        LLVMValueRef first_elem_ptr = LLVMBuildInBoundsGEP2(llvm->builder,
            llvm_type(llvm->context, from_type), array_ptr, indices, 2, "array_first_elem");

        LLVMValueRef view_struct = build_view_struct(llvm, to_type, length, first_elem_ptr);

        if (out_val != nullptr)
            *out_val = view_struct;
        return;
    }
    else if (from_type == ast_type_builtin(TYPE_UNINIT))
    {
        // From "uninit" coercion
        if (out_val != nullptr)
            *out_val = nullptr;
        return;
    }

    // Determine coercion kind
    ast_coercion_kind_t kind = ast_type_can_coerce(from_type, to_type);

    if (kind == COERCION_WIDEN || kind == COERCION_SIGNEDNESS)
    {
        // Integer coercion: WIDEN or SIGNEDNESS
        LLVMValueRef value = nullptr;
        ast_visitor_visit(llvm, coercion->expr, &value);

        LLVMTypeRef target_llvm_type = llvm_type(llvm->context, to_type);
        size_t from_size = ast_type_sizeof(from_type);
        size_t to_size = ast_type_sizeof(to_type);

        LLVMValueRef result;
        if (from_size < to_size)
        {
            // Extend based on TARGET type signedness
            // - If extending to unsigned (e.g., i32 -> usize), zero-extend
            // - If extending to signed (e.g., u32 -> i64), use source signedness
            // For COERCION_WIDEN (same signedness), this naturally does the right thing
            // For COERCION_SIGNEDNESS to unsigned target, we zero-extend
            if (kind == COERCION_SIGNEDNESS && !ast_type_is_signed(to_type))
            {
                // Converting to unsigned: always zero-extend (e.g., i32 -> usize)
                result = LLVMBuildZExt(llvm->builder, value, target_llvm_type, "zext");
            }
            else if (ast_type_is_signed(from_type))
            {
                // Same signedness widening, or signed->signed: sign-extend
                result = LLVMBuildSExt(llvm->builder, value, target_llvm_type, "sext");
            }
            else
            {
                // Unsigned source: zero-extend
                result = LLVMBuildZExt(llvm->builder, value, target_llvm_type, "zext");
            }
        }
        else if (from_size > to_size)
        {
            // Truncate
            result = LLVMBuildTrunc(llvm->builder, value, target_llvm_type, "trunc");
        }
        else
        {
            // Same size, just signedness change (no-op in LLVM's type system)
            result = value;
        }

        if (out_val != nullptr)
            *out_val = result;
        return;
    }

    panic("coercion from %s to %s not implemented", ast_type_string(from_type), ast_type_string(to_type));
}

static void emit_construct_expr(void* self_, ast_construct_expr_t* construct, void* out_)
{
    llvm_codegen_t* llvm = self_;
    LLVMValueRef* out = out_;
    panic_if(out == nullptr);

    const char* class_name = construct->class_type->data.user.name;
    class_layout_t* layout = hash_table_find(&llvm->class_layouts, class_name);
    panic_if(layout == nullptr);

    LLVMTypeRef class_type = llvm_type(llvm->context, construct->class_type);
    LLVMValueRef instance = LLVMBuildAlloca(llvm->builder, class_type, "construct");

    for (size_t i = 0; i < vec_size(&construct->member_inits); ++i)
    {
        ast_member_init_t* init = vec_get(&construct->member_inits, i);

        panic_if(!hash_table_contains(&layout->member_indices, init->member_name));
        intptr_t index = (intptr_t)hash_table_find(&layout->member_indices, init->member_name);

        LLVMValueRef init_val = nullptr;
        ast_visitor_visit(llvm, init->init_expr, &init_val);

        LLVMValueRef ptr_to_member = LLVMBuildStructGEP2(llvm->builder, class_type, instance, index, init->member_name);
        LLVMBuildStore(llvm->builder, init_val, ptr_to_member);
    }

    *out = instance;
}

static void emit_paren_expr(void* self_, ast_paren_expr_t* paren, void* out_)
{
    llvm_codegen_t* llvm = self_;

    ast_visitor_visit(llvm, paren->expr, out_);
}

static void emit_ref_expr(void* self_, ast_ref_expr_t* ref, void* out_)
{
    llvm_codegen_t* llvm = self_;
    LLVMValueRef* out = out_;
    panic_if(out == nullptr);

    // For function names, we need to look up the function in the module
    if (llvm->function_name)
    {
        LLVMValueRef fn = LLVMGetNamedFunction(llvm->module, ref->name);
        panic_if(fn == nullptr);
        *out = fn;
        return;
    }

    // Look up variable in symbol table
    LLVMValueRef alloca_ref = hash_table_find(llvm->symbols, ref->name);
    panic_if(alloca_ref == nullptr);

    // If we need an lvalue (address), return the alloca directly
    if (llvm->lvalue)
    {
        *out = alloca_ref;
    }
    else
    {
        // Otherwise, load the value from the alloca
        LLVMTypeRef var_type = llvm_type(llvm->context, ref->base.type);
        *out = LLVMBuildLoad2(llvm->builder, var_type, alloca_ref, ref->name);
    }
}

static void emit_self_expr(void* self_, ast_self_expr_t* self_expr, void* out_)
{
    llvm_codegen_t* llvm = self_;
    LLVMValueRef* out_val = out_;
    panic_if(out_val == nullptr);

    // Look up 'self' in symbol table (stored during method setup)
    LLVMValueRef self_alloc = hash_table_find(llvm->symbols, "self");
    panic_if(self_alloc == nullptr);

    // If we need an lvalue (address), return the alloca directly
    if (llvm->lvalue)
    {
        *out_val = self_alloc;
    }
    else
    {
        // Otherwise, load the self pointer value from the alloca
        LLVMTypeRef self_type = llvm_type(llvm->context, self_expr->base.type);
        *out_val = LLVMBuildLoad2(llvm->builder, self_type, self_alloc, "self");
    }
}

static void emit_unary_op(void* self_, ast_unary_op_t* unary, void* out_)
{
    llvm_codegen_t* llvm = self_;
    LLVMValueRef* out = out_;

    switch (unary->op)
    {
        case TOKEN_AMPERSAND:
        {
            llvm->lvalue = true;
            ast_visitor_visit(llvm, unary->expr, out);
            llvm->lvalue = false;
            break;
        }
        case TOKEN_STAR:
        {
            // When dereferencing, we always need the pointer value (rvalue)
            // even if we're in an lvalue context (e.g., *ptr = value)
            bool output_lvalue = llvm->lvalue;
            llvm->lvalue = false;
            LLVMValueRef ptr_value = nullptr;
            ast_visitor_visit(llvm, unary->expr, &ptr_value);
            llvm->lvalue = output_lvalue;

            if (llvm->lvalue)
            {
                // Return the pointer value itself (the address to store to)
                *out = ptr_value;
            }
            else
            {
                // Load from the pointer
                LLVMTypeRef deref_type = llvm_type(llvm->context, unary->base.type);
                *out = LLVMBuildLoad2(llvm->builder, deref_type, ptr_value, "deref");
            }
            break;
        }
        default:
            panic("Unhandled unary op: %d", unary->op);
    }
}

static void emit_uninit_lit(void* self_, ast_uninit_lit_t* uninit, void* out_)
{
    (void)self_;
    (void)uninit;
    LLVMValueRef* out_val = out_;

    if (out_val != nullptr)
        *out_val = nullptr;
}

static void emit_compound_stmt(void* self_, ast_compound_stmt_t* block, void* out_)
{
    for (size_t i = 0; i < vec_size(&block->inner_stmts); ++i)
        ast_visitor_visit(self_, vec_get(&block->inner_stmts, i), out_);
}

static void emit_decl_stmt(void* self_, ast_decl_stmt_t* stmt, void* out_)
{
    ast_visitor_visit(self_, stmt->decl, out_);
}

static void emit_expr_stmt(void* self_, ast_expr_stmt_t* stmt, void* out_)
{
    ast_visitor_visit(self_, stmt->expr, out_);
}

static void emit_if_stmt(void* self_, ast_if_stmt_t* stmt, void* out_)
{
    llvm_codegen_t* llvm = self_;

    set_debug_location(llvm, AST_NODE(stmt));

    // Evaluate condition
    LLVMValueRef cond_val = nullptr;
    ast_visitor_visit(llvm, stmt->condition, &cond_val);

    // Create basic blocks for then, else, and join
    LLVMBasicBlockRef then_block = LLVMAppendBasicBlock(llvm->current_function, "if.then");
    LLVMBasicBlockRef else_block = LLVMAppendBasicBlock(llvm->current_function, "if.else");
    LLVMBasicBlockRef join_block = LLVMAppendBasicBlock(llvm->current_function, "if.join");

    // Conditional branch based on condition
    LLVMBuildCondBr(llvm->builder, cond_val, then_block, else_block);

    // Emit then branch
    LLVMPositionBuilderAtEnd(llvm->builder, then_block);
    ast_visitor_visit(llvm, stmt->then_branch, out_);
    // Only add branch if current block doesn't already have a terminator
    // Note: after emitting then branch, builder may be in a different block (e.g., bounds check safe block)
    LLVMBasicBlockRef current_block = LLVMGetInsertBlock(llvm->builder);
    if (LLVMGetBasicBlockTerminator(current_block) == nullptr)
        LLVMBuildBr(llvm->builder, join_block);

    // Emit else branch
    LLVMPositionBuilderAtEnd(llvm->builder, else_block);
    if (stmt->else_branch != nullptr)
        ast_visitor_visit(llvm, stmt->else_branch, out_);
    // Only add branch if current block doesn't already have a terminator
    current_block = LLVMGetInsertBlock(llvm->builder);
    if (LLVMGetBasicBlockTerminator(current_block) == nullptr)
        LLVMBuildBr(llvm->builder, join_block);

    // Continue after if-else
    LLVMPositionBuilderAtEnd(llvm->builder, join_block);
}

static void emit_return_stmt(void* self_, ast_return_stmt_t* stmt, void* out_)
{
    llvm_codegen_t* llvm = self_;
    (void)out_;

    set_debug_location(llvm, AST_NODE(stmt));

    if (stmt->value_expr != nullptr)
    {
        LLVMValueRef return_val = nullptr;
        ast_visitor_visit(llvm, stmt->value_expr, &return_val);
        LLVMBuildRet(llvm->builder, return_val);
    }
    else
    {
        LLVMBuildRetVoid(llvm->builder);
    }
}

static void emit_while_stmt(void* self_, ast_while_stmt_t* stmt, void* out_)
{
    llvm_codegen_t* llvm = self_;

    set_debug_location(llvm, AST_NODE(stmt));

    // Create basic blocks for condition, body, and end
    LLVMBasicBlockRef cond_block = LLVMAppendBasicBlock(llvm->current_function, "while.cond");
    LLVMBasicBlockRef body_block = LLVMAppendBasicBlock(llvm->current_function, "while.body");
    LLVMBasicBlockRef end_block = LLVMAppendBasicBlock(llvm->current_function, "while.end");

    // Branch to condition block
    LLVMBuildBr(llvm->builder, cond_block);

    // Emit condition block
    LLVMPositionBuilderAtEnd(llvm->builder, cond_block);
    LLVMValueRef cond_val = nullptr;
    ast_visitor_visit(llvm, stmt->condition, &cond_val);
    LLVMBuildCondBr(llvm->builder, cond_val, body_block, end_block);

    // Emit body block
    LLVMPositionBuilderAtEnd(llvm->builder, body_block);
    ast_visitor_visit(llvm, stmt->body, out_);
    // Only add branch if current block doesn't already have a terminator
    // Note: after emitting body, builder may be in a different block (e.g., bounds check safe block)
    LLVMBasicBlockRef current_block = LLVMGetInsertBlock(llvm->builder);
    if (LLVMGetBasicBlockTerminator(current_block) == nullptr)
        LLVMBuildBr(llvm->builder, cond_block);

    // Continue after while loop
    LLVMPositionBuilderAtEnd(llvm->builder, end_block);
}

static void class_layout_destroy(void* layout_)
{
    class_layout_t* layout = layout_;
    if (layout == nullptr)
        return;

    free(layout->class_name);
    hash_table_deinit(&layout->member_indices);
    vec_deinit(&layout->member_names);
    vec_deinit(&layout->member_types);
    free(layout);
}

llvm_codegen_t* llvm_codegen_create()
{
    llvm_codegen_t* llvm = malloc(sizeof(*llvm));

    // Initialize LLVM C API objects
    llvm->context = LLVMContextCreate();
    llvm->module = LLVMModuleCreateWithNameInContext("shiro_module", llvm->context);
    llvm->builder = LLVMCreateBuilderInContext(llvm->context);
    llvm->current_function = nullptr;

    // NOTE: We do not need to init the visitor because we override every implementation
    *llvm = (llvm_codegen_t){
        .context = llvm->context,
        .module = llvm->module,
        .builder = llvm->builder,
        .presenter = ast_presenter_create(),
        .class_layouts = HASH_TABLE_INIT(class_layout_destroy),
        .base = (ast_visitor_t){
            .visit_root = emit_root,
            // Declarations
            .visit_param_decl = emit_param_decl,
            .visit_var_decl = emit_var_decl,
            // Definitions
            .visit_class_def = emit_class_def,
            .visit_fn_def = emit_fn_def,
            .visit_method_def = emit_method_def,
            // Expressions
            .visit_array_lit = emit_array_lit,
            .visit_array_slice = emit_array_slice,
            .visit_array_subscript = emit_array_subscript,
            .visit_bin_op = emit_bin_op,
            .visit_bool_lit = emit_bool_lit,
            .visit_call_expr = emit_call_expr,
            .visit_coercion_expr = emit_coercion_expr,
            .visit_construct_expr = emit_construct_expr,
            .visit_float_lit = emit_float_lit,
            .visit_int_lit = emit_int_lit,
            .visit_member_access = emit_member_access,
            .visit_member_init = emit_member_init,
            .visit_method_call = emit_method_call,
            .visit_null_lit = emit_null_lit,
            .visit_paren_expr = emit_paren_expr,
            .visit_ref_expr = emit_ref_expr,
            .visit_self_expr = emit_self_expr,
            .visit_unary_op = emit_unary_op,
            .visit_uninit_lit = emit_uninit_lit,
            // .visit_str_lit = emit_str_lit, FIXME:
            // Statements
            .visit_compound_stmt = emit_compound_stmt,
            .visit_decl_stmt = emit_decl_stmt,
            .visit_expr_stmt = emit_expr_stmt,
            .visit_if_stmt = emit_if_stmt,
            .visit_return_stmt = emit_return_stmt,
            .visit_while_stmt = emit_while_stmt,
        },
    };

    return llvm;
}

void llvm_codegen_destroy(llvm_codegen_t* llvm)
{
    if (llvm == nullptr)
        return;

    // Clean up debug info (if initialized)
    if (llvm->di_scopes != nullptr)
        hash_table_destroy(llvm->di_scopes);
    if (llvm->di_builder != nullptr)
        LLVMDisposeDIBuilder(llvm->di_builder);

    // Clean up LLVM C API objects
    if (llvm->builder != nullptr)
        LLVMDisposeBuilder(llvm->builder);
    if (llvm->module != nullptr)
        LLVMDisposeModule(llvm->module);
    if (llvm->context != nullptr)
        LLVMContextDispose(llvm->context);

    ast_presenter_destroy(llvm->presenter);
    hash_table_deinit(&llvm->class_layouts);

    free(llvm);
}

void llvm_codegen_generate(llvm_codegen_t* llvm, ast_node_t* root, const char* source_filename, FILE* out)
{
    // Register builtin functions
    register_builtins(llvm);

    // Initialize debug info
    llvm->di_builder = LLVMCreateDIBuilder(llvm->module);

    // Extract filename from path (get part after last '/')
    const char* filename = strrchr(source_filename, '/');
    filename = filename ? filename + 1 : source_filename;

    // Extract directory (everything up to last '/')
    const char* last_slash = strrchr(source_filename, '/');
    size_t dir_len = last_slash ? (size_t)(last_slash - source_filename) : 0;
    char* directory = dir_len > 0 ? strndup(source_filename, dir_len) : strdup(".");

    // Create debug info file
    llvm->di_file = LLVMDIBuilderCreateFile(llvm->di_builder, filename, strlen(filename), directory, strlen(directory));

    // Create compile unit
    llvm->di_compile_unit = LLVMDIBuilderCreateCompileUnit(llvm->di_builder, LLVMDWARFSourceLanguageC,
        llvm->di_file, "ShiroC Compiler", 16, 0, "", 0, 0, "", 0, LLVMDWARFEmissionFull, 0, 0, 0, "", 0, "", 0);

    llvm->di_scopes = hash_table_create(nullptr);
    llvm->current_di_scope = nullptr;

    // Generate the LLVM IR into the module
    ast_visitor_visit(llvm, root, nullptr);

    // Finalize debug info
    LLVMDIBuilderFinalize(llvm->di_builder);

    // Add module flags for debug info version (required by LLVM)
    LLVMMetadataRef debug_info_version = LLVMValueAsMetadata(
        LLVMConstInt(LLVMInt32TypeInContext(llvm->context), 3, false));
    LLVMMetadataRef dwarf_version = LLVMValueAsMetadata(
        LLVMConstInt(LLVMInt32TypeInContext(llvm->context), 4, false));
    LLVMAddModuleFlag(llvm->module, LLVMModuleFlagBehaviorWarning, "Dwarf Version", 13, dwarf_version);
    LLVMAddModuleFlag(llvm->module, LLVMModuleFlagBehaviorWarning, "Debug Info Version", 18, debug_info_version);

    // Print the module to a string
    char* ir_string = LLVMPrintModuleToString(llvm->module);
    fprintf(out, "%s", ir_string);
    LLVMDisposeMessage(ir_string);

    free(directory);
}
