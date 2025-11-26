module;
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>

export module llvm;


export namespace llvm {
    using ::llvm::BasicBlock;
    using ::llvm::Instruction;
    using ::llvm::Value;
    using ::llvm::LLVMContext;
    using ::llvm::Module;
    using ::llvm::IRBuilder;
    using ::llvm::PHINode;
    using ::llvm::Function;
    using ::llvm::FunctionType;
    using ::llvm::Type;
    using ::llvm::Constant;
    using ::llvm::PointerType;
    using ::llvm::ArrayType;
    using ::llvm::ConstantInt;
    using ::llvm::APInt;

    namespace Intrinsic {
        using ::llvm::Intrinsic::getOrInsertDeclaration;
        using ::llvm::Intrinsic::IndependentIntrinsics;
    }
}
