#include "llvm_codegen.h"

#include "ast/decl/param_decl.h"
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

#include <llvm-c/Core.h>
#include <stdlib.h>

struct llvm_codegen
{
    ast_visitor_t base;

    LLVMContextRef context;
    LLVMModuleRef module;
    LLVMBuilderRef builder;
    LLVMValueRef current_function;

    ast_presenter_t* presenter;
    hash_table_t* symbols;  // name (char*) -> LLVMValueRef (alloca)

    bool lvalue;
    bool address_of_lvalue;
    bool function_name;
};


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

    LLVMValueRef alloc_ref = LLVMBuildAlloca(llvm->builder, llvm_type(llvm->context, var->type), var->name);

    if (var->init_expr != nullptr)
    {
        LLVMValueRef init_value;
        ast_visitor_visit(llvm, var->init_expr, &init_value);
        LLVMBuildStore(llvm->builder, init_value, alloc_ref);
    }

    hash_table_insert(llvm->symbols, var->name, alloc_ref);
    *out_ref = alloc_ref;
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

    // Add entry block and position builder
    LLVMBasicBlockRef entry_block = LLVMAppendBasicBlock(fn_val, "entry");
    LLVMPositionBuilderAtEnd(llvm->builder, entry_block);

    // Allocate space for all parameters
    for (size_t i = 0; i < vec_size(&fn_def->params); ++i)
        ast_visitor_visit(llvm, vec_get(&fn_def->params, i), LLVMGetParam(fn_val, i));

    LLVMValueRef out_val;
    ast_visitor_visit(llvm, fn_def->body, &out_val);

    if (fn_def->return_type == ast_type_builtin(TYPE_VOID))
        LLVMBuildRetVoid(llvm->builder);

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

static void emit_null_lit(void* self_, ast_null_lit_t* lit, void* out_)
{
    llvm_codegen_t* llvm = self_;
    LLVMValueRef* out_val = out_;

    LLVMTypeRef null_type = llvm_type(llvm->context, lit->base.type);
    *out_val = LLVMConstNull(null_type);
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
            LLVMValueRef ptr_value = nullptr;
            ast_visitor_visit(llvm, unary->expr, &ptr_value);

            if (llvm->lvalue)
            {
                *out = ptr_value;
            }
            else
            {
                LLVMTypeRef deref_type = llvm_type(llvm->context, unary->base.type);
                *out = LLVMBuildLoad2(llvm->builder, deref_type, ptr_value, "deref");
            }
            break;
        }
        default:
            panic("Unhandled unary op: %d", unary->op);
    }
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
    // Only add branch if block doesn't already have a terminator
    if (LLVMGetBasicBlockTerminator(then_block) == nullptr)
        LLVMBuildBr(llvm->builder, join_block);

    // Emit else branch
    LLVMPositionBuilderAtEnd(llvm->builder, else_block);
    if (stmt->else_branch != nullptr)
        ast_visitor_visit(llvm, stmt->else_branch, out_);
    // Only add branch if block doesn't already have a terminator
    if (LLVMGetBasicBlockTerminator(else_block) == nullptr)
        LLVMBuildBr(llvm->builder, join_block);

    // Continue after if-else
    LLVMPositionBuilderAtEnd(llvm->builder, join_block);
}

static void emit_return_stmt(void* self_, ast_return_stmt_t* stmt, void* out_)
{
    llvm_codegen_t* llvm = self_;
    (void)out_;

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
    // Only add branch if block doesn't already have a terminator
    if (LLVMGetBasicBlockTerminator(body_block) == nullptr)
        LLVMBuildBr(llvm->builder, cond_block);

    // Continue after while loop
    LLVMPositionBuilderAtEnd(llvm->builder, end_block);
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
        .current_function = nullptr,
        .presenter = ast_presenter_create(),
        .symbols = nullptr,
        .lvalue = false,
        .address_of_lvalue = false,
        .function_name = false,
        .base = (ast_visitor_t){
            .visit_root = emit_root,
            // Declarations
            .visit_param_decl = emit_param_decl,
            .visit_var_decl = emit_var_decl,
            // Definitions
            .visit_fn_def = emit_fn_def,
            // Expressions
            .visit_bin_op = emit_bin_op,
            .visit_bool_lit = emit_bool_lit,
            .visit_call_expr = emit_call_expr,
            .visit_float_lit = emit_float_lit,
            .visit_int_lit = emit_int_lit,
            .visit_null_lit = emit_null_lit,
            .visit_paren_expr = emit_paren_expr,
            .visit_ref_expr = emit_ref_expr,
            .visit_unary_op = emit_unary_op,
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

    // Clean up LLVM C API objects
    if (llvm->builder != nullptr)
        LLVMDisposeBuilder(llvm->builder);
    if (llvm->module != nullptr)
        LLVMDisposeModule(llvm->module);
    if (llvm->context != nullptr)
        LLVMContextDispose(llvm->context);

    ast_presenter_destroy(llvm->presenter);

    free(llvm);
}

void llvm_codegen_generate(llvm_codegen_t* llvm, ast_node_t* root, FILE* out)
{
    // Generate the LLVM IR into the module
    ast_visitor_visit(llvm, root, nullptr);

    // Print the module to a string
    char* ir_string = LLVMPrintModuleToString(llvm->module);
    fprintf(out, "%s", ir_string);
    LLVMDisposeMessage(ir_string);
}
