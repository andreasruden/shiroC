#ifndef LLVM_CODEGEN__H
#define LLVM_CODEGEN__H

#include <llvm-c/Core.h>
#include <stdio.h>

typedef struct ast_node ast_node_t;
typedef struct semantic_context semantic_context_t;
typedef struct llvm_codegen llvm_codegen_t;

llvm_codegen_t* llvm_codegen_create(const char* project_name, const char* module_name);

void llvm_codegen_destroy(llvm_codegen_t* llvm);

void llvm_codegen_init(llvm_codegen_t* llvm, const char* module_name, semantic_context_t* sema_ctx);

void llvm_codegen_add_ast(llvm_codegen_t* llvm, ast_node_t* root, const char* source_filename);

void llvm_codegen_finalize(llvm_codegen_t* llvm, FILE* out);

#endif
