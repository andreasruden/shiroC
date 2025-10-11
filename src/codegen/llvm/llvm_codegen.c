#include "llvm_codegen.h"

#include "ast/node.h"
#include "ast/type.h"
#include "ast/util/presenter.h"
#include "ast/visitor.h"
#include "common/containers/hash_table.h"
#include "common/containers/vec.h"
#include "common/debug/panic.h"
#include "common/util//ssprintf.h"
#include "parser/lexer.h"

#include <stdlib.h>
#include <string.h>

struct llvm_codegen
{
    ast_visitor_t base;
    ast_presenter_t* presenter;
    hash_table_t* parameters;  // TODO: This should be a hash-set
    int indentation;
    int label_count;
    int temporary_count;
    bool lvalue;
    bool function_name;
};

#define EMIT_INLINE(fmt, ...) do { \
    fprintf((FILE*)out_, fmt __VA_OPT__(,) __VA_ARGS__); \
} while (0)

#define EMIT(fmt, ...) do { \
    fprintf((FILE*)out_, "%*s" fmt, 2 * ((llvm_codegen_t*)self_)->indentation, "" __VA_OPT__(,) __VA_ARGS__); \
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

#define TMPVAR() ssprintf("%%t%d", ((llvm_codegen_t*)self_)->temporary_count)

#define NEW_TMPVAR() ({ \
    ++((llvm_codegen_t*)self_)->temporary_count; \
    TMPVAR(); \
})

#define TMPVAR_SAVE() ({ \
    char* var_tmp = TMPVAR(); \
    char* var_str = malloc(strlen(var_tmp) + 1); \
    strcpy(var_str, var_tmp); \
    var_str; \
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
    if (type == ast_type_from_builtin(TYPE_BOOL))
        return "i1";
    return ast_type_string(type);
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
    (void)self_;

    EMIT_INLINE("%s %%%s", llvm_type(param->type), param->name);
}

static void emit_var_decl(void* self_, ast_var_decl_t* var, void* out_)
{
    EMIT_PRELUDE(var);

    EMITLN("%%%s = alloca %s", var->name, llvm_type(var->type));
}

static void emit_fn_def(void* self_, ast_fn_def_t* fn_def, void* out_)
{
    EMIT_PRELUDE(fn_def);

    llvm->temporary_count = 0;
    llvm->label_count = 0;
    llvm->parameters = hash_table_create(nullptr);

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
        hash_table_insert(llvm->parameters, param->name, nullptr);
        EMITLN("%%%s.addr = alloca %s", param->name, llvm_type(param->type));
        EMITLN("store %s %%%s, %s* %%%s.addr", llvm_type(param->type), param->name, llvm_type(param->type),
            param->name);
    }
    EMIT("\n");

    ast_visitor_visit(llvm, fn_def->body, out_);
    if (fn_def->return_type == ast_type_from_builtin(TYPE_VOID))
        EMITLN("ret void");
    --llvm->indentation;
    EMITLN("}\n");

    hash_table_destroy(llvm->parameters);
    llvm->parameters = nullptr;
}

static void emit_bool_lit(void* self_, ast_bool_lit_t* lit, void* out_)
{
    if (lit->value)
        EMIT("%s = add i1 0, 1", NEW_TMPVAR());
    else
        EMIT("%s = add i1 0, 0", NEW_TMPVAR());
    EMIT_INLINE("  ");
    EMIT_SRC_INLINE(lit);
}

static void emit_float_lit(void* self_, ast_float_lit_t* lit, void* out_)
{
    if (lit->base.type == ast_type_from_builtin(TYPE_F64))
        EMIT("%s = add %s 0, %lf", NEW_TMPVAR(), llvm_type(lit->base.type), lit->value);
    else
        EMIT("%s = add %s 0, %f", NEW_TMPVAR(), llvm_type(lit->base.type), (float)lit->value);
    EMIT_INLINE("  ");
    EMIT_SRC_INLINE(lit);
}

static void emit_int_lit(void* self_, ast_int_lit_t* lit, void* out_)
{
    if (ast_type_is_signed(lit->base.type))
        EMIT("%s = add %s 0, %ld", NEW_TMPVAR(), llvm_type(lit->base.type), lit->value.as_signed);
    else
        EMIT("%s = add %s 0, %lu", NEW_TMPVAR(), llvm_type(lit->base.type), lit->value.as_unsigned);
    EMIT_INLINE("  ");
    EMIT_SRC_INLINE(lit);
}

static void emit_simple_assignment(void* self_, ast_bin_op_t* bin_op, void* out_)
{
    EMIT_PRELUDE(bin_op);

    ast_visitor_visit(llvm, bin_op->rhs, out_);

    EMITLN("; =");
    // Store value into var
    EMIT("store %s %s, %s* ", llvm_type(bin_op->lhs->type), TMPVAR(), llvm_type(bin_op->lhs->type));
    llvm->lvalue = true;
    ast_visitor_visit(llvm, bin_op->lhs, out_);
    llvm->lvalue = false;

    EMIT("\n");
}

static void emit_other_bin_op(void* self_, ast_bin_op_t* bin_op, void* out_)
{
    EMIT_PRELUDE(bin_op);

    ast_visitor_visit(llvm, bin_op->lhs, out_);
    char* lhs_val = TMPVAR_SAVE();

    ast_visitor_visit(llvm, bin_op->rhs, out_);
    char* rhs_val = TMPVAR_SAVE();

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
    EMITLN("%s = %s%s %s %s, %s", NEW_TMPVAR(), pre_op, op, llvm_type(bin_op->lhs->type), lhs_val, rhs_val);

    if (token_type_is_assignment_op(bin_op->op))
    {
        EMIT("store %s %s, %s* ", llvm_type(bin_op->lhs->type), TMPVAR(), llvm_type(bin_op->lhs->type));
        llvm->lvalue = true;
        ast_visitor_visit(llvm, bin_op->lhs, out_);
        llvm->lvalue = false;
        EMIT_INLINE("\n");
    }

    free(lhs_val);
    free(rhs_val);
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
    vec_t arg_vars = VEC_INIT(free);
    for (size_t i = 0; i < vec_size(&call->arguments); ++i)
    {
        // TODO: It should be easier to lookup the function at this point, so that I can insert the name of the
        //       parameter here.
        ast_visitor_visit(llvm, vec_get(&call->arguments, i), out_);
        vec_push(&arg_vars, TMPVAR_SAVE());
    }

    // Emit name of function
    EMITLN("; Call");
    if (call->base.type == ast_type_from_builtin(TYPE_VOID))
        EMIT("call void @");
    else
        EMIT("%s = call %s @", NEW_TMPVAR(), llvm_type(call->base.type));

    llvm->lvalue = llvm->function_name = true;
    ast_visitor_visit(llvm, call->function, out_);
    llvm->lvalue = llvm->function_name = false;

    // Emit tmpvar for every earlier derived argument
    EMIT_INLINE("(");
    for (size_t i = 0; i < vec_size(&call->arguments); ++i)
    {
        ast_expr_t* expr = vec_get(&call->arguments, i);
        const char* tmpvar = vec_get(&arg_vars, i);
        EMIT_INLINE("%s %s", llvm_type(expr->type), tmpvar);
        if (i + 1 < vec_size(&call->arguments))
            EMIT_INLINE(", ");
    }
    EMIT_INLINE(")\n");

    vec_deinit(&arg_vars);
}

static void emit_paren_expr(void* self_, ast_paren_expr_t* paren, void* out_)
{
    EMIT_PRELUDE(paren);

    ast_visitor_visit(llvm, paren->expr, out_);
}

static void emit_ref_expr(void* self_, ast_ref_expr_t* ref, void* out_)
{
    llvm_codegen_t* llvm = self_;

    if (llvm->lvalue)
    {
        if (llvm->function_name)
            EMIT_INLINE("%s", ref->name);
        else if (hash_table_contains(llvm->parameters, ref->name))
            EMIT_INLINE("%%%s.addr", ref->name);
        else
            EMIT_INLINE("%%%s", ref->name);
    }
    else
    {
        if (hash_table_contains(llvm->parameters, ref->name))
        {
            EMIT("%s = load %s, %s* %%%s.addr", NEW_TMPVAR(), llvm_type(ref->base.type), llvm_type(ref->base.type),
                ref->name);
            EMIT_INLINE("  ");
            EMIT_SRC_INLINE(ref);
        }
        else
        {
            EMIT("%s = load %s, %s* %%%s", NEW_TMPVAR(), llvm_type(ref->base.type), llvm_type(ref->base.type),
                ref->name);
            EMIT_INLINE("  ");
            EMIT_SRC_INLINE(ref);
        }
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

    ast_visitor_visit(llvm, stmt->condition, out_);
    EMIT("\n");
    EMITLN("br i1 %s, label %%%s, label %%%s\n", TMPVAR(), then_label, else_label);

    EMIT_LABEL(then_label);
    ast_visitor_visit(llvm, stmt->then_branch, out_);
    EMITLN("br label %%%s\n", join_label);

    EMIT_LABEL(else_label);
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

    ast_visitor_visit(self_, stmt->value_expr, out_);
    EMITLN("ret %s %s", llvm_type(stmt->value_expr->type), TMPVAR());
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
    ast_visitor_visit(llvm, stmt->condition, out_);
    EMIT("\n");
    EMITLN("br i1 %s, label %%%s, label %%%s\n", TMPVAR(), body_label, end_label);

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
            .visit_paren_expr = emit_paren_expr,
            .visit_ref_expr = emit_ref_expr,
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

    free(llvm);
}

void llvm_codegen_generate(llvm_codegen_t* llvm, ast_node_t* root, FILE* out)
{
    ast_visitor_visit(llvm, root, out);
}
