#include "llvm_codegen.h"

#include "ast/node.h"
#include "ast/type.h"
#include "ast/util/presenter.h"
#include "ast/visitor.h"
#include "common/containers/hash_table.h"
#include "common/containers/string.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "common/util//ssprintf.h"
#include "parser/lexer.h"

#include <stdlib.h>
#include <string.h>

struct llvm_codegen
{
    ast_visitor_t base;
    FILE* emit_to;
    ast_presenter_t* presenter;
    hash_table_t* symbols;  // name (char*) -> alloca register name (char*)
    int indentation;
    int temporary_count;
    int label_count;
    bool lvalue;
    bool address_of_lvalue;
    bool function_name;

    vec_t gc;  // garbage collection
};

#define EMIT_INLINE(fmt, ...) do { \
    fprintf((FILE*)((llvm_codegen_t*)self_)->emit_to, fmt __VA_OPT__(,) __VA_ARGS__); \
} while (0)

#define EMIT(fmt, ...) do { \
    fprintf((FILE*)((llvm_codegen_t*)self_)->emit_to, "%*s" fmt, \
        2 * ((llvm_codegen_t*)self_)->indentation, "" __VA_OPT__(,) __VA_ARGS__); \
} while (0)

#define EMITLN(fmt, ...) EMIT(fmt "\n" __VA_OPT__(,) __VA_ARGS__)

#define EMIT_LABEL(label) EMIT_INLINE("%s:\n", label);

#define NEW_LABEL(label) ({ \
    char* label_tmp = ssprintf("%s_%d", label, ((llvm_codegen_t*)self_)->label_count); \
    ++((llvm_codegen_t*)self_)->label_count; \
    char* label_str = malloc(strlen(label_tmp) + 1); \
    strcpy(label_str, label_tmp); \
    label_str; \
})

#define EMIT_SRC_INLINE(node) do { \
    char* src_for_node__ = source_code_for(((llvm_codegen_t*)self_), node); \
    EMIT_INLINE("; %s\n", src_for_node__); \
    free(src_for_node__); \
} while (0)

#define EMIT_SRC(node) do { \
    char* src_for_node__ = source_code_for(((llvm_codegen_t*)self_), node); \
    EMIT("; %s\n", src_for_node__); \
    free(src_for_node__); \
} while (0)

#define EMIT_PRELUDE(node) \
    (void)out_; \
    llvm_codegen_t* llvm = self_; \
    (void)llvm; \
    EMIT_SRC(node)

// Return source code representation for node. Returned code is always one line.
static char* source_code_for(llvm_codegen_t* llvm, void* node)
{
    return ast_presenter_present_node(llvm->presenter, node);
}

static const char* llvm_type(ast_type_t* type)
{
    if (type == nullptr)
        return "void";
    else if (type == ast_type_builtin(TYPE_BOOL))
        return "i1";
    else if (type == ast_type_builtin(TYPE_U8))
        return "i8";
    else if (type == ast_type_builtin(TYPE_U16))
        return "i16";
    else if (type == ast_type_builtin(TYPE_U32))
        return "i32";
    else if (type == ast_type_builtin(TYPE_U64))
        return "i64";
    else if (type == ast_type_builtin(TYPE_F32))
        return "float";
    else if (type == ast_type_builtin(TYPE_F64))
        return "double";

    return ast_type_string(type);
}

static char* new_tmpvar(llvm_codegen_t* llvm)
{
    string_t tmp = STRING_INIT;
    string_append_cstr(&tmp, ssprintf("%%t.%d", llvm->temporary_count));
    ++llvm->temporary_count;
    char* str = string_release(&tmp);
    vec_push(&llvm->gc, str);
    return str;
}

static void emit_root(void* self_, ast_root_t* root, void* out_)
{
    EMIT_PRELUDE(root);
    EMIT("\n\n");

    for (size_t i = 0; i < vec_size(&root->tl_defs); ++i)
        ast_visitor_visit(llvm, vec_get(&root->tl_defs, i), out_);

    EMIT("\n");
}

static void emit_param_decl(void* self_, ast_param_decl_t* param, void* out_)
{
    (void)out_;
    (void)self_;

    EMIT_INLINE("%s %%%s", llvm_type(param->type), param->name);
}

static void emit_var_decl(void* self_, ast_var_decl_t* var, void* out_)
{
    EMIT_PRELUDE(var);

    ast_type_t* var_type = var->type == nullptr ? var->init_expr->type : var->type;
    EMITLN("%%%s = alloca %s", var->name, llvm_type(var_type));

    if (var->init_expr != nullptr)
    {
        char* ssa_val = nullptr;
        ast_visitor_visit(llvm, var->init_expr, &ssa_val);
        EMITLN("store %s %s, %s* %%%s", llvm_type(var_type), ssa_val, llvm_type(var_type), var->name);
    }

    hash_table_insert(llvm->symbols, var->name, strdup(ssprintf("%%%s", var->name)));
}

static void emit_fn_def(void* self_, ast_fn_def_t* fn_def, void* out_)
{
    EMIT_PRELUDE(fn_def);

    llvm->temporary_count = 0;
    llvm->label_count = 0;
    llvm->symbols = hash_table_create(free);

    EMIT("define %s @%s(", llvm_type(fn_def->return_type), fn_def->base.name);
    for (size_t i = 0; i < vec_size(&fn_def->params); ++i)
    {
        ast_visitor_visit(llvm, vec_get(&fn_def->params, i), out_);
        if (i + 1 < vec_size(&fn_def->params))
            EMIT(", ");
    }

    EMITLN(") {");
    EMITLN("entry:");
    ++llvm->indentation;

    EMITLN("; Allocate space for all parameters");
    for (size_t i = 0; i < vec_size(&fn_def->params); ++i)
    {
        ast_param_decl_t* param = vec_get(&fn_def->params, i);
        string_t tmp = STRING_INIT;
        string_append_cstr(&tmp, ssprintf("%%%s.addr", param->name));
        EMITLN("%s = alloca %s", string_cstr(&tmp), llvm_type(param->type));
        EMITLN("store %s %%%s, %s* %s", llvm_type(param->type), param->name, llvm_type(param->type),
            string_cstr(&tmp));
        hash_table_insert(llvm->symbols, param->name, string_release(&tmp));
    }
    EMIT("\n");

    ast_visitor_visit(llvm, fn_def->body, out_);
    if (fn_def->return_type == ast_type_builtin(TYPE_VOID))
        EMITLN("ret void");
    --llvm->indentation;
    EMITLN("}\n");

    hash_table_destroy(llvm->symbols);
    llvm->symbols = nullptr;
}

static void emit_bool_lit(void* self_, ast_bool_lit_t* lit, void* out_)
{
    EMIT_PRELUDE(lit);
    char** ssa_val = out_;

    if (lit->value)
        *ssa_val = "true";
    else
        *ssa_val = "false";
}

static void emit_float_lit(void* self_, ast_float_lit_t* lit, void* out_)
{
    EMIT_PRELUDE(lit);
    char** ssa_val = out_;

    string_t tmp = STRING_INIT;
    if (lit->base.type == ast_type_builtin(TYPE_F32))
        string_append_cstr(&tmp, ssprintf("%f", (float)lit->value));
    else
        string_append_cstr(&tmp, ssprintf("%lf", lit->value));

    *ssa_val = string_release(&tmp);
    vec_push(&llvm->gc, *ssa_val);
}

static void emit_int_lit(void* self_, ast_int_lit_t* lit, void* out_)
{
    EMIT_PRELUDE(lit);
    char** ssa_val = out_;

    string_t tmp = STRING_INIT;
    if (ast_type_is_signed(lit->base.type))
        string_append_cstr(&tmp, ssprintf("%ld", lit->value.as_signed));
    else
        string_append_cstr(&tmp, ssprintf("%lu", lit->value.as_unsigned));

    *ssa_val = string_release(&tmp);
    vec_push(&llvm->gc, *ssa_val);
}

static void emit_null_lit(void* self_, ast_null_lit_t* lit, void* out_)
{
    EMIT_PRELUDE(lit);
    char** ssa_val = out_;

    *ssa_val = "null";
}

static void emit_simple_assignment(void* self_, ast_bin_op_t* bin_op, void* out_)
{
    EMIT_PRELUDE(bin_op);

    char* ssa_val = nullptr;
    ast_visitor_visit(llvm, bin_op->rhs, &ssa_val);

    EMITLN("; =");

    llvm->lvalue = true;
    char* ssa_reg = nullptr;
    ast_visitor_visit(llvm, bin_op->lhs, &ssa_reg);
    llvm->lvalue = false;

    // Store value into var
    EMITLN("store %s %s, %s* %s", llvm_type(bin_op->lhs->type), ssa_val, llvm_type(bin_op->lhs->type), ssa_reg);
}

static void emit_other_bin_op(void* self_, ast_bin_op_t* bin_op, void* out_)
{
    EMIT_PRELUDE(bin_op);

    char* ssa_val_lhs = nullptr;
    ast_visitor_visit(llvm, bin_op->lhs, &ssa_val_lhs);

    char* ssa_val_rhs = nullptr;
    ast_visitor_visit(llvm, bin_op->rhs, &ssa_val_rhs);

    const char* pre_op = "";
    const char* op;
    switch (bin_op->op)
    {
        case TOKEN_PLUS_ASSIGN:
        case TOKEN_PLUS: op = "add"; break;
        case TOKEN_MINUS_ASSIGN:
        case TOKEN_MINUS: op = "sub"; break;
        case TOKEN_MUL_ASSIGN:
        case TOKEN_STAR: op = "mul"; break;
        case TOKEN_DIV_ASSIGN:
        case TOKEN_DIV: op = "sdiv"; break;     // TODO: udiv
        case TOKEN_MODULO_ASSIGN:
        case TOKEN_MODULO: op = "srem"; break;  // TODO: urem
        // TODO: float
        // TODO: unsigned versions
        case TOKEN_LT: op = "slt"; pre_op = "icmp "; break;
        case TOKEN_LTE: op = "sle"; pre_op = "icmp "; break;
        case TOKEN_GT: op = "sgt"; pre_op = "icmp "; break;
        case TOKEN_GTE: op = "sge"; pre_op = "icmp "; break;
        case TOKEN_EQ: op = "eq"; pre_op = "icmp "; break;
        case TOKEN_NEQ: op = "ne"; pre_op = "icmp "; break;
        default:
            panic("Unhandled arithmetic op: %d", bin_op->op);
    }

    EMITLN("; %s", token_type_str(bin_op->op));
    char* output_val = new_tmpvar(llvm);
    EMITLN("%s = %s%s %s %s, %s", output_val, pre_op, op, llvm_type(bin_op->lhs->type), ssa_val_lhs,
        ssa_val_rhs);

    if (token_type_is_assignment_op(bin_op->op))
    {
        llvm->lvalue = true;
        char* ssa_val = nullptr;
        ast_visitor_visit(llvm, bin_op->lhs, &ssa_val);
        llvm->lvalue = false;
        EMIT("store %s %s, %s* %s", llvm_type(bin_op->lhs->type), output_val, llvm_type(bin_op->lhs->type), ssa_val);
    }

    if (out_ != nullptr)
        *((char**)out_) = output_val;
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
    EMIT_PRELUDE(call);

    // Emit expression to derive arguments
    EMITLN("; Arguments");
    vec_t arg_ssa_vals = VEC_INIT(nullptr);
    for (size_t i = 0; i < vec_size(&call->arguments); ++i)
    {
        // TODO: It should be easier to lookup the function at this point, so that I can insert the name of the
        //       parameter here.
        char* ssa_val = nullptr;
        ast_visitor_visit(llvm, vec_get(&call->arguments, i), &ssa_val);
        vec_push(&arg_ssa_vals, ssa_val);
    }

    // Emit name of function
    EMITLN("; Call");
    llvm->function_name = true;
    char* fn_name = nullptr;
    ast_visitor_visit(llvm, call->function, &fn_name);
    llvm->function_name = false;
    if (call->base.type == ast_type_builtin(TYPE_VOID))
        EMIT("call void @%s", fn_name);
    else
    {
        char** ssa_val_out = out_;
        char* tmp_var = new_tmpvar(llvm);
        EMIT("%s = call %s @%s", tmp_var, llvm_type(call->base.type), fn_name);
        if (ssa_val_out != nullptr)
            *ssa_val_out = tmp_var;
    }

    // Emit tmpvar for every earlier derived argument
    EMIT_INLINE("(");
    for (size_t i = 0; i < vec_size(&call->arguments); ++i)
    {
        ast_expr_t* expr = vec_get(&call->arguments, i);
        const char* ssa_val = vec_get(&arg_ssa_vals, i);
        EMIT_INLINE("%s %s", llvm_type(expr->type), ssa_val);
        if (i + 1 < vec_size(&call->arguments))
            EMIT_INLINE(", ");
    }
    EMIT_INLINE(")\n");

    vec_deinit(&arg_ssa_vals);
}

static void emit_paren_expr(void* self_, ast_paren_expr_t* paren, void* out_)
{
    EMIT_PRELUDE(paren);

    ast_visitor_visit(llvm, paren->expr, out_);
}

static void emit_ref_expr(void* self_, ast_ref_expr_t* ref, void* out_)
{
    llvm_codegen_t* llvm = self_;
    const char** out = out_;
    panic_if(out == nullptr);

    const char* ssa_val = hash_table_find(llvm->symbols, ref->name);
    panic_if(!llvm->function_name && ssa_val == nullptr);

    if (llvm->function_name)
    {
        *out = ref->name;
    }
    else if (llvm->lvalue)
    {
        *out = ssa_val;
    }
    else
    {
        *out = new_tmpvar(llvm);
        EMIT("%s = load %s, %s* %s", *out, llvm_type(ref->base.type), llvm_type(ref->base.type),
            ssa_val);
        EMIT_INLINE("  ");
        EMIT_SRC_INLINE(ref);
    }
}

static void unary_addr_of(void* self_, ast_unary_op_t* unary, char** out)
{
    llvm_codegen_t* llvm = self_;

    llvm->lvalue = true;
    char* ssa_val = nullptr;
    ast_visitor_visit(llvm, unary->expr, &ssa_val);
    llvm->lvalue = false;
    *out = ssa_val;
}

static void unary_deref(void* self_, ast_unary_op_t* unary, char** out)
{
    llvm_codegen_t* llvm = self_;

    char* ssa_val = nullptr;
    ast_visitor_visit(llvm, unary->expr, &ssa_val);

    if (llvm->lvalue)
    {
        *out = ssa_val;
        return;
    }

    *out = new_tmpvar(llvm);
    EMITLN("%s = load %s, %s* %s", *out, llvm_type(unary->base.type), llvm_type(unary->base.type), ssa_val);
}

static void emit_unary_op(void* self_, ast_unary_op_t* unary, void* out_)
{
    EMIT_PRELUDE(unary);
    char** out = out_;

    switch (unary->op)
    {
        case TOKEN_AMPERSAND:
        {
            unary_addr_of(llvm, unary, out);
            break;
        }
        case TOKEN_STAR:
        {
            unary_deref(llvm, unary, out);
            break;
        }
        default:
            panic("Unhandled unary op: %d", unary->op);
    }
}

static void emit_compound_stmt(void* self_, ast_compound_stmt_t* block, void* out_)
{
    (void)self_;

    for (size_t i = 0; i < vec_size(&block->inner_stmts); ++i)
    {
        ast_visitor_visit(self_, vec_get(&block->inner_stmts, i), out_);
        EMIT("\n");
    }
}

static void emit_decl_stmt(void* self_, ast_decl_stmt_t* stmt, void* out_)
{
    (void)self_;

    ast_visitor_visit(self_, stmt->decl, out_);
}

static void emit_expr_stmt(void* self_, ast_expr_stmt_t* stmt, void* out_)
{
    (void)self_;

    ast_visitor_visit(self_, stmt->expr, out_);
}

static void emit_if_stmt(void* self_, ast_if_stmt_t* stmt, void* out_)
{
    llvm_codegen_t* llvm = self_;

    EMITLN("; If start line=%d", AST_NODE(stmt)->source_begin.line);

    char* then_label = NEW_LABEL("if_then");
    char* else_label = NEW_LABEL("if_else");
    char* join_label = NEW_LABEL("if_join");

    char* ssa_val = nullptr;
    ast_visitor_visit(llvm, stmt->condition, &ssa_val);
    EMIT("\n");
    EMITLN("br i1 %s, label %%%s, label %%%s\n", ssa_val, then_label, else_label);

    EMIT_LABEL(then_label);
    ast_visitor_visit(llvm, stmt->then_branch, out_);
    EMITLN("br label %%%s\n", join_label);

    EMIT_LABEL(else_label);
    if (stmt->else_branch != nullptr)
        ast_visitor_visit(llvm, stmt->else_branch, out_);
    EMITLN("br label %%%s\n", join_label);

    EMIT_LABEL(join_label);

    EMITLN("; If end line=%d", AST_NODE(stmt)->source_end.line);

    free(then_label);
    free(else_label);
    free(join_label);
}

static void emit_return_stmt(void* self_, ast_return_stmt_t* stmt, void* out_)
{
    EMIT_PRELUDE(stmt);

    char* ssa_val = nullptr;
    ast_visitor_visit(self_, stmt->value_expr, &ssa_val);
    EMITLN("ret %s %s", llvm_type(stmt->value_expr->type), ssa_val);
}

static void emit_while_stmt(void* self_, ast_while_stmt_t* stmt, void* out_)
{
    llvm_codegen_t* llvm = self_;

    char* cond_label = NEW_LABEL("while_cond");
    char* body_label = NEW_LABEL("while_body");
    char* end_label = NEW_LABEL("while_end");

    EMITLN("; While start line=%d", AST_NODE(stmt)->source_begin.line);
    EMITLN("br label %%%s\n", cond_label);

    EMIT_LABEL(cond_label);
    char* ssa_val = nullptr;
    ast_visitor_visit(llvm, stmt->condition, &ssa_val);
    EMIT("\n");
    EMITLN("br i1 %s, label %%%s, label %%%s\n", ssa_val, body_label, end_label);

    EMIT_LABEL(body_label);
    ast_visitor_visit(llvm, stmt->body, out_);
    EMITLN("br label %%%s\n", cond_label);

    EMIT_LABEL(end_label);
    EMITLN("; While end line=%d", AST_NODE(stmt)->source_end.line);

    free(cond_label);
    free(body_label);
    free(end_label);
}

llvm_codegen_t* llvm_codegen_create()
{
    llvm_codegen_t* llvm = malloc(sizeof(*llvm));

    // NOTE: We do not need to init the visitor because we override every implementation
    *llvm = (llvm_codegen_t){
        .presenter = ast_presenter_create(),
        .gc = VEC_INIT(free),
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

    ast_presenter_destroy(llvm->presenter);
    vec_deinit(&llvm->gc);

    free(llvm);
}

void llvm_codegen_generate(llvm_codegen_t* llvm, ast_node_t* root, FILE* out)
{
    llvm->emit_to = out;
    ast_visitor_visit(llvm, root, nullptr);
}
