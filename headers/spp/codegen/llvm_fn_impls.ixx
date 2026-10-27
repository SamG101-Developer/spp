module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_fn_impls;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct TypeAst);
use(spp::asts::meta, struct CompilerMetaData);
use(spp::codegen, struct LlvmCtx);

export namespace spp::codegen::fn_impls {
  /// The binary operation enums that are used for all the
  /// maths and boolean lowering functions. All standard math
  /// is "checked" for safety. Shareable across the signature
  /// "(T, T) -> T" or "(T, T) -> Bool".
  enum class BinOp {
    Add, Sub, Mul, SDiv, UDiv, SRem, URem, Shl, LShr, Or, And, Xor,
    ICmpEQ, ICmpNE, ICmpSLT, ICmpULT, ICmpSLE, ICmpULE, ICmpSGT, ICmpUGT, ICmpSGE, ICmpUGE,
    FCmpOEQ, FCmpONE, FCmpOLT, FCmpOLE, FCmpOGT, FCmpOGE,
    FAdd, FSub, FMul, FDiv, FRem,
    SAddChecked, UAddChecked, SSubChecked, USubChecked, SMulChecked, UMulChecked,
  };

  /// The unary arithmetic operations shareable across the
  /// signature "(T) -> T".
  enum class UnOp {
    Neg, Not, FNeg
  };

  /// Value-conversion operations: "(Src) -> Dest", where
  /// "Src" and "Dest" can differ.
  enum class ConvOp {
    SIToFP, UIToFP, FPTrunc, Trunc, SExt, ZExt, FPExt, BitCast, FPToSI, FPToUI
  };

  /// Atomic read-modify-write operations shareable across
  /// "Atom[T]::fetch_*"/"exchange".
  enum class AtomicRmwOp {
    Xchg, Add, Sub, And, Nand, Or, Xor, Max, Min, UMax, UMin, FAdd, FSub, FMax, FMin
  };

  /// ======================================================
  /// Layer 1: function + entry-block creation. The root every
  /// other layer is built on.
  /// ======================================================

  /// Create the mangled LLVM function for the prototype with
  /// the given signature, plus its entry block, and leave the
  /// builder's insert point set there. This lifts out the
  /// name/function-type/function-create/entry-block boilerplate
  /// that would otherwise need to be added to every hand-written
  /// intrinsic wrapper function.
  auto SimpleCreateFn(
    SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ret_ty, Vec<llvm::Type*> const &param_tys) -> llvm::Function*;

  /// ======================================================
  /// Layer 2: one builder per repeated function-body "shape",
  /// parameterized by a scoped enum (no closures, no macros,
  /// no templates) identifying which operation to apply.
  /// ======================================================

  /// True if "op" is a comparison, and therefore returns
  /// "Bool" (i1) rather than the operand type.
  auto IsCompareBinOp(BinOp op) -> bool;

  /// True if "op" is a shift, whose distance operand is
  /// separately typed in the source and so needs coercing.
  auto IsShiftBinOp(BinOp op) -> bool;

  /// Build the actual instruction for a "BinOp" on operands
  /// "a" and "b".
  auto ApplyBinOp(LlvmCtx *ctx, BinOp op, llvm::Value *a, llvm::Value *b) -> llvm::Value*;

  /// Build the actual instruction for a "UnOp" on operand "a".
  auto ApplyUnOp(LlvmCtx *ctx, UnOp op, llvm::Value *a) -> llvm::Value*;

  /// Build the actual instruction for a "ConvOp" converting
  /// "a" to "dest_ty".
  auto ApplyConvOp(LlvmCtx *ctx, ConvOp op, llvm::Value *a, llvm::Type *dest_ty) -> llvm::Value*;

  /// "(T, T) -> T" (or "-> Bool" for comparison ops): apply
  /// a "BinOp" to the two incoming arguments and return it.
  auto SimpleIntrinsicBinop(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, BinOp op) -> void;

  /// "(&mut T, T) -> Void": load the current value out of the
  /// first (pointer) argument, apply a "BinOp" against the
  /// second argument, and store the result back - the "_assign"
  /// (compound-assignment) shape.
  auto SimpleIntrinsicBinopAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, BinOp op) -> void;

  /// "(T) -> T": apply a "UnOp" to the incoming argument and
  /// return it.
  auto SimpleIntrinsicUnop(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, UnOp op) -> void;

  /// "(&mut T) -> Void": load the current value out of the
  /// (pointer) argument, apply a "UnOp", and store it back.
  auto SimpleIntrinsicUnopAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, UnOp op) -> void;

  /// "(Src) -> Dest": apply a "ConvOp" to the incoming
  /// argument. Unlike every other builder here, the return
  /// type is not "ty" - it's derived from "proto"'s own
  /// declared return type, since conversions genuinely go
  /// from one type to a different one (e.g. "S32 -> F64");
  /// "ty" is only the source/operand type.
  auto SimpleIntrinsicConv(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, ConvOp op) -> void;

  /// "(T) -> Bool": compare the incoming argument for
  /// equality against a fixed constant (e.g. "is_zero"/
  /// "is_one").
  auto SimpleIntrinsicIsConst(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, bool is_float, double value) -> void;

  /// Map an "AtomicRmwOp" to the underlying
  /// "llvm::AtomicRMWInst::BinOp". Todo: Just use
  /// the original enum?
  auto ApplyAtomicRmwOp(AtomicRmwOp op) -> llvm::AtomicRMWInst::BinOp;

  /// "(&self, val: T, order: U8) -> T": The atomic methods
  /// "Atom[T]::fetch_*"/"exchange" all share this exact
  /// shape - atomically apply an "AtomicRmwOp" between
  /// "self.val" and "val", returning "self.val"'s value
  /// from before the operation (which is exactly what
  /// "llvm.atomicrmw" itself returns, so no extra load/
  /// store logic is needed).
  auto SimpleAtomicFetchRmw(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, AtomicRmwOp op) -> void;

  /// "(T, T) -> T": call a two-operand LLVM intrinsic directly
  /// (e.g. "llvm.smax") and return its result as-is - for
  /// intrinsics whose result type genuinely is "T" (unlike,
  /// say, the "with.overflow" family below). "rhs_ty" is the
  /// second operand's type when it is not "T" ("(T, S32) -> T"
  /// for "llvm.powi").
  auto SimpleBinaryIntrinsicCall(
    SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, llvm::Intrinsic::IndependentIntrinsics intrinsic,
    llvm::Type *rhs_ty = nullptr) -> void;

  /// "(T, T) -> (T, Bool)": call a two-operand "with.overflow"-
  /// shaped LLVM intrinsic (e.g. "llvm.sadd.with.overflow"),
  /// whose result is already the literal struct "{T, i1}" that
  /// "(T, Bool)" lowers to, so it's returned as-is.
  auto SimpleBinaryIntrinsicCallOverflow(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty,
    llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void;

  /// "(T) -> T": call a one-operand LLVM intrinsic directly
  /// (e.g. "llvm.sqrt") and return its result.
  auto SimpleUnaryIntrinsicCall(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty,
    llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void;

  /// "(T) -> T", ignoring the argument entirely: always return
  /// the given (already-computed) constant value. Used for
  /// every "zero-arg" builtin (neg_one/zero/one/two/min_val/
  /// max_val/...) - S++ still synthesizes a dummy one-argument
  /// signature for these regardless of true arity, matching
  /// every other builder here.
  auto SimpleGetValue(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, llvm::Value *val) -> void;

  /// ======================================================
  /// Layer 2b: coroutine-specific shared helpers (still "one
  /// shape, many callers", just not enum-driven since each
  /// is only reused by a forwards/backwards pair rather than
  /// a whole family of operations).
  /// ======================================================

  /// Shared codegen for a coroutine that hands out the elements
  /// of a fixed-size, inline array one at a time, moving each
  /// element out via a "gen"-style suspend/resume point.
  /// The array's length is a compile-time constant, so this
  /// unrolls into one yield per element (forwards or backwards)
  /// instead of a runtime loop, matching exactly what a hand-
  /// written "gen self[i]" loop would lower to. Used by
  /// "Arr::iter_mov" and "Arr::reverse_iter_mov" only; views
  /// use runtime length so they have their own lowering.
  auto SimpleCoroIter(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, bool reverse, bool borrow) -> void;

  /// Shared codegen for a coroutine that hands out the elements
  /// of a dynamically-sized view one at a time, using a loop
  /// and counter strategy.
  auto SimpleCoroViewIter(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, bool reverse, bool borrow) -> void;

  /// Shared logic for the two NonNull forwarding calls, to &T
  /// and &mut T - in S++ they are different but in LLVM it's the
  /// same pointer technique: Todo: different attributes/flags?
  auto SimpleCoroNonNullFwd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx) -> void;

  /// Shared forwarding for providing a forwarding call to a
  /// View of a collection, like a vector or array. Again, the
  /// two forwarding calls for any collections will share this.
  /// This is the core of the following two functions.
  auto SimpleCoroContiguousFwd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Value *data, llvm::Value *length) -> void;

  /// The array wrapper of the above lowering codegen for the
  /// contiguous view forwarding.
  auto SimpleCoroArrayFwd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx) -> void;

  /// The vector wrapper of the above lowering codegen for the
  /// contiguous view forwarding.
  auto SimpleCoroVectorFwd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx) -> void;

  /// Shared codegen for indexing a view, based on an index. It
  /// does the (provably-safe) pointer math and GEPs the element
  /// from the pointer/length.
  auto SimpleCoroViewIndex(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx) -> void;

  /// Shared codegen for slicing a view, based on two bounds. It
  /// does the (provably-safe) pointer math and GEPs the slice from
  /// the pointer/length.
  auto SimpleCoroViewSlice(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx) -> void;

  /// ======================================================
  /// Layer 3: the builtins with a lowering of their own. One
  /// that is a single call to a Layer 2 builder is bound to it
  /// where it is registered ("builtins.cpp") instead.
  /// ======================================================

  // Fixed values (SimpleGetValue)
  auto StdArrayNew(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNumFloatNegOne(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNumFloatZero(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNumFloatOne(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNumIntNegOne(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNumIntZero(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNumIntOne(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNumIntTwo(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsMinVal(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsMaxVal(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFminVal(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFmaxVal(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // Two-operand intrinsics with a shape of their own
  auto StdIntrinsicsFpowi(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSshlSaturating(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUshlSaturating(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // One-operand intrinsics with a shape of their own
  auto StdIntrinsicsCtlz(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdDebugBreakpointInternal(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // Three-way integer comparisons (bespoke: two-type-overloaded intrinsic, operand type read off "this")
  auto StdIntrinsicsScmp(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUcmp(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // Bespoke: needs a genuinely custom shape (two different argument types + Bool return)
  auto StdIntrinsicsFpclass(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // Bespoke: coroutines / arrays / vectors / slots / futures / memory / atomics (Layer 1, or a Layer 2b helper)

  auto StdGeneratorSend(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto StdGeneratorDrop(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdGeneratorOnceSend(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto StdFunctionFunMovDrop(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdCffiCClosureFrom(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNonNullRead(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNonNullWrite(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNonNullRaw(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNonNullEraseType(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNonNullCast(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNonNullFromPtrInner(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto StdVolRead(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdVolWrite(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdVolReplace(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto StdRawBufTakeAt(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdRawBufPlaceAt(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdRawBufShift(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdRawBufClearRange(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto StdMemOpsSizeOf(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdMemOpsAlignOf(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdMemOpsSizeOfVal(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdMemOpsAlignOfVal(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdMemOpsReplace(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdMemOpsSwap(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdMemOpsDrop(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdMemOpsDropInPlace(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto StdThreadingAtomicIsLockFree(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicFenceInner(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicLoadInner(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicStoreInner(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicCompexInner(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicCompexWeakInner(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto StdThreadingAtomicFetchNot(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
}
