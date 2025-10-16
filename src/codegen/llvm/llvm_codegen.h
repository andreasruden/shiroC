#ifndef LLVM_CODEGEN__H
#define LLVM_CODEGEN__H

#include <llvm-c/Core.h>
#include <stdio.h>

typedef struct ast_node ast_node_t;
typedef struct llvm_codegen llvm_codegen_t;

llvm_codegen_t* llvm_codegen_create();

void llvm_codegen_destroy(llvm_codegen_t* llvm);

void llvm_codegen_generate(llvm_codegen_t* llvm, ast_node_t* root, FILE* out);

#endif
