#ifndef LLVM_TYPE_UTILS_H
#define LLVM_TYPE_UTILS_H

#include <llvm-c/Core.h>

typedef struct ast_type ast_type_t;

// Convert AST type to LLVM type
LLVMTypeRef llvm_type(LLVMContextRef ctx, ast_type_t* type);

#endif
