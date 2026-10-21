module;
#include <llvm/ADT/StringSet.h>
#include <llvm/CodeGen/MachineFunction.h>
#include <llvm/CodeGen/TargetSubtargetInfo.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DiagnosticInfo.h>
#include <llvm/IR/DiagnosticPrinter.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/Transforms/Utils/Local.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

export module llvm;

export namespace llvm {
  // The free operators ("StringRef == StringRef", "Twine +"):
  // found by argument-dependent lookup only if exported.
  using ::llvm::operator==;
  using ::llvm::operator!=;
  using ::llvm::operator<;
  using ::llvm::operator+;

  using ::llvm::cast;
  using ::llvm::dyn_cast;
  using ::llvm::dyn_cast_or_null;
  using ::llvm::errs;
  using ::llvm::outs;
  using ::llvm::isa;
  using ::llvm::install_fatal_error_handler;
  using ::llvm::verifyFunction;
  using ::llvm::verifyModule;
  using ::llvm::appendToUsed;
  using ::llvm::changeToInvokeAndSplitBasicBlock;
  using ::llvm::consumeError;
  using ::llvm::instructions;
  using ::llvm::toString;
  using ::llvm::InitializeAllAsmPrinters;
  using ::llvm::InitializeAllTargetInfos;
  using ::llvm::InitializeAllTargetMCs;
  using ::llvm::InitializeAllTargets;
  using ::llvm::InitializeNativeTarget;
  using ::llvm::InitializeNativeTargetAsmPrinter;
  using ::llvm::raw_fd_ostream;
  using ::llvm::raw_string_ostream;
  using ::llvm::fltSemantics;

  using ::llvm::Align;
  using ::llvm::AllocaInst;
  using ::llvm::APInt;
  using ::llvm::APFloat;
  using ::llvm::APFloatBase;
  using ::llvm::ArrayRef;
  using ::llvm::ArrayType;
  using ::llvm::AtomicOrdering;
  using ::llvm::AtomicRMWInst;
  using ::llvm::Attribute;
  using ::llvm::BasicBlock;
  using ::llvm::CallBase;
  using ::llvm::CallInst;
  using ::llvm::CodeGenFileType;
  using ::llvm::Constant;
  using ::llvm::ConstantAsMetadata;
  using ::llvm::ConstantArray;
  using ::llvm::ConstantDataArray;
  using ::llvm::ConstantInt;
  using ::llvm::ConstantFP;
  using ::llvm::ConstantPointerNull;
  using ::llvm::ConstantTokenNone;
  using ::llvm::ConstantStruct;
  using ::llvm::DiagnosticInfo;
  using ::llvm::DiagnosticPrinterRawOStream;
  using ::llvm::Function;
  using ::llvm::FunctionType;
  using ::llvm::GlobalValue;
  using ::llvm::GlobalVariable;
  using ::llvm::IRBuilder;
  using ::llvm::Instruction;
  using ::llvm::IntegerType;
  using ::llvm::LLVMContext;
  using ::llvm::MachineFunction;
  using ::llvm::MaybeAlign;
  using ::llvm::MDNode;
  using ::llvm::MDString;
  using ::llvm::Metadata;
  using ::llvm::Module;
  using ::llvm::OperandBundleDef;
  using ::llvm::PointerType;
  using ::llvm::PHINode;
  using ::llvm::SmallVector;
  using ::llvm::StringLiteral;
  using ::llvm::StringRef;
  using ::llvm::StringSet;
  using ::llvm::StructType;
  using ::llvm::Target;
  using ::llvm::TargetMachine;
  using ::llvm::TargetOptions;
  using ::llvm::TargetRegistry;
  using ::llvm::TargetSubtargetInfo;
  using ::llvm::Triple;
  using ::llvm::Type;
  using ::llvm::TypeSize;
  using ::llvm::PoisonValue;
  using ::llvm::UndefValue;
  using ::llvm::UWTableKind;
  using ::llvm::Value;

  using ::llvm::DS_Warning;
  using ::llvm::DS_Error;

  namespace sys {
    using ::llvm::sys::getDefaultTargetTriple;
  }

  namespace sys::fs {
    using ::llvm::sys::fs::OpenFlags;
    using ::llvm::sys::fs::OF_None;
  }

  namespace legacy {
    using ::llvm::legacy::PassManager;
  }

  namespace mdconst {
    using ::llvm::mdconst::extract;
  }

  namespace object {
    using ::llvm::object::Binary;
    using ::llvm::object::ELF32BE;
    using ::llvm::object::ELF32LE;
    using ::llvm::object::ELF64BE;
    using ::llvm::object::ELF64LE;
    using ::llvm::object::ELFObjectFile;
    using ::llvm::object::ELFObjectFileBase;
    using ::llvm::object::ObjectFile;
    using ::llvm::object::SymbolRef;
  }

  namespace ELF {
    using ::llvm::ELF::DF_1_NOW;
    using ::llvm::ELF::DF_BIND_NOW;
    using ::llvm::ELF::DT_BIND_NOW;
    using ::llvm::ELF::DT_FLAGS;
    using ::llvm::ELF::DT_FLAGS_1;
    using ::llvm::ELF::PF_W;
    using ::llvm::ELF::PF_X;
    using ::llvm::ELF::PT_GNU_RELRO;
    using ::llvm::ELF::PT_LOAD;
  }

  namespace PICLevel {
    using ::llvm::PICLevel::BigPIC;
  }

  namespace PIELevel {
    using ::llvm::PIELevel::Large;
  }

  namespace Reloc {
    using ::llvm::Reloc::PIC_;
  }

  namespace Intrinsic {
    using ::llvm::Intrinsic::getName;
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
    using ::llvm::Intrinsic::debugtrap;

    using ::llvm::Intrinsic::sadd_with_overflow;
    using ::llvm::Intrinsic::uadd_with_overflow;
    using ::llvm::Intrinsic::smul_with_overflow;
    using ::llvm::Intrinsic::umul_with_overflow;
    using ::llvm::Intrinsic::ssub_with_overflow;
    using ::llvm::Intrinsic::usub_with_overflow;

    using ::llvm::Intrinsic::coro_alloc;
    using ::llvm::Intrinsic::coro_begin;
    using ::llvm::Intrinsic::coro_destroy;
    using ::llvm::Intrinsic::coro_done;
    using ::llvm::Intrinsic::coro_end;
    using ::llvm::Intrinsic::coro_free;
    using ::llvm::Intrinsic::coro_id;
    using ::llvm::Intrinsic::coro_promise;
    using ::llvm::Intrinsic::coro_resume;
    using ::llvm::Intrinsic::coro_size;
    using ::llvm::Intrinsic::coro_suspend;
  }
}
