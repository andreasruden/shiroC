#include "llvm_type_utils.h"

#include "ast/type.h"
#include "common/debug/panic.h"

#include <llvm-c/Types.h>
#include <llvm-c/Core.h>

LLVMTypeRef llvm_type(LLVMContextRef ctx, ast_type_t* type)
{
    if (type == nullptr)
        return LLVMVoidTypeInContext(ctx);

    switch (type->kind)
    {
        case AST_TYPE_BUILTIN:
        {
            switch (type->data.builtin.type)
            {
                case TYPE_VOID:
                    return LLVMVoidTypeInContext(ctx);
                case TYPE_BOOL:
                    return LLVMInt1TypeInContext(ctx);
                case TYPE_I8:
                case TYPE_U8:
                    return LLVMInt8TypeInContext(ctx);
                case TYPE_I16:
                case TYPE_U16:
                    return LLVMInt16TypeInContext(ctx);
                case TYPE_I32:
                case TYPE_U32:
                    return LLVMInt32TypeInContext(ctx);
                case TYPE_I64:
                case TYPE_U64:
                case TYPE_ISIZE:  // FIXME: See FIXME in ast_type_sizeof
                case TYPE_USIZE:
                    return LLVMInt64TypeInContext(ctx);
                case TYPE_F32:
                    return LLVMFloatTypeInContext(ctx);
                case TYPE_F64:
                    return LLVMDoubleTypeInContext(ctx);
                case TYPE_NULL:
                    // TODO: NULL type should have been coerced to a pointer type during semantic analysis.
                    // This is a workaround - we should fix the semantic analyzer to properly coerce null
                    // in all contexts (especially in comparisons).
                    // For now, treat it as a generic pointer (opaque)
                    return LLVMPointerTypeInContext(ctx, 0);
                case TYPE_UNINIT:
                case TYPE_END:
                    panic("Unsupported builtin type for LLVM codegen: %d", type->data.builtin.type);
            }
            break;
        }
        case AST_TYPE_POINTER:
        {
            return LLVMPointerTypeInContext(ctx, 0);
        }
        case AST_TYPE_ARRAY:
        {
            LLVMTypeRef element_type = llvm_type(ctx, type->data.array.element_type);
            return LLVMArrayType(element_type, (unsigned int)type->data.array.size);
        }
        case AST_TYPE_VIEW:
        {
            LLVMTypeRef size_type = LLVMInt64TypeInContext(ctx);  // FIXME: word-sized & unsigned
            LLVMTypeRef element_type = LLVMPointerTypeInContext(ctx, 0);
            LLVMTypeRef fields[] = { size_type, element_type };
            return LLVMStructTypeInContext(ctx, fields, 2, false);
        }
        case AST_TYPE_INVALID:
        case AST_TYPE_USER:
        case AST_TYPE_HEAP_ARRAY:
            panic("Unsupported type kind for LLVM codegen: %d", type->kind);
    }

    panic("Unknown type kind: %d", type->kind);
}
