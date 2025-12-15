module;
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

export module llvm;


export namespace llvm {
    using ::llvm::cast;
    using ::llvm::dyn_cast;
    using ::llvm::isa;
    using ::llvm::raw_fd_ostream;
    using ::llvm::fltSemantics;

    using ::llvm::Align;
    using ::llvm::APInt;
    using ::llvm::APFloat;
    using ::llvm::APFloatBase;
    using ::llvm::ArrayType;
    using ::llvm::Attribute;
    using ::llvm::BasicBlock;
    using ::llvm::Constant;
    using ::llvm::ConstantArray;
    using ::llvm::ConstantDataArray;
    using ::llvm::ConstantInt;
    using ::llvm::ConstantFP;
    using ::llvm::ConstantPointerNull;
    using ::llvm::ConstantTokenNone;
    using ::llvm::ConstantStruct;
    using ::llvm::Function;
    using ::llvm::FunctionType;
    using ::llvm::GlobalValue;
    using ::llvm::GlobalVariable;
    using ::llvm::IRBuilder;
    using ::llvm::Instruction;
    using ::llvm::LLVMContext;
    using ::llvm::MaybeAlign;
    using ::llvm::Module;
    using ::llvm::PointerType;
    using ::llvm::PHINode;
    using ::llvm::StructType;
    using ::llvm::Triple;
    using ::llvm::Type;
    using ::llvm::UndefValue;
    using ::llvm::Value;

    namespace sys::fs {
        using ::llvm::sys::fs::OpenFlags;
    }

    namespace Intrinsic {
        using ::llvm::Intrinsic::getOrInsertDeclaration;
        using ::llvm::Intrinsic::IndependentIntrinsics;

        using ::llvm::Intrinsic::abs;
        using ::llvm::Intrinsic::acos;
        using ::llvm::Intrinsic::asin;
        using ::llvm::Intrinsic::atan;
        using ::llvm::Intrinsic::atan2;
        using ::llvm::Intrinsic::ceil;
        using ::llvm::Intrinsic::cos;
        using ::llvm::Intrinsic::exp;
        using ::llvm::Intrinsic::exp2;
        using ::llvm::Intrinsic::floor;
        using ::llvm::Intrinsic::log;
        using ::llvm::Intrinsic::log10;
        using ::llvm::Intrinsic::log2;
        using ::llvm::Intrinsic::pow;
        using ::llvm::Intrinsic::sin;
        using ::llvm::Intrinsic::sqrt;
        using ::llvm::Intrinsic::tan;
        using ::llvm::Intrinsic::ctlz;

        using ::llvm::Intrinsic::memset;
        using ::llvm::Intrinsic::trap;

        using ::llvm::Intrinsic::sadd_with_overflow;
        using ::llvm::Intrinsic::uadd_with_overflow;
        using ::llvm::Intrinsic::smul_with_overflow;
        using ::llvm::Intrinsic::umul_with_overflow;
        using ::llvm::Intrinsic::ssub_with_overflow;
        using ::llvm::Intrinsic::usub_with_overflow;

        using ::llvm::Intrinsic::coro_begin;
        using ::llvm::Intrinsic::coro_destroy;
        using ::llvm::Intrinsic::coro_done;
        using ::llvm::Intrinsic::coro_id;
        using ::llvm::Intrinsic::coro_resume;
        using ::llvm::Intrinsic::coro_suspend;
    }
}
