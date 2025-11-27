module;
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>

export module llvm;


export namespace llvm {
    using ::llvm::cast;
    using ::llvm::APInt;
    using ::llvm::APFloat;
    using ::llvm::APFloatBase;
    using ::llvm::ArrayType;
    using ::llvm::BasicBlock;
    using ::llvm::Constant;
    using ::llvm::ConstantInt;
    using ::llvm::ConstantFP;
    using ::llvm::ConstantPointerNull;
    using ::llvm::ConstantTokenNone;
    using ::llvm::Function;
    using ::llvm::FunctionType;
    using ::llvm::IRBuilder;
    using ::llvm::Instruction;
    using ::llvm::LLVMContext;
    using ::llvm::Module;
    using ::llvm::PointerType;
    using ::llvm::PHINode;
    using ::llvm::StructType;
    using ::llvm::Type;
    using ::llvm::Value;

    namespace Intrinsic {
        using ::llvm::Intrinsic::getOrInsertDeclaration;
        using ::llvm::Intrinsic::IndependentIntrinsics;
    }
}
