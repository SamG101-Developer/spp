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
  /// say, the "with.overflow" family below).
  auto SimpleBinaryIntrinsicCall(
    SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void;

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
  /// Layer 3: individual builtin implementations, grouped by
  /// which Layer 2 builder (if any) they use.
  /// ======================================================

  // BinOp (SimpleIntrinsicBinop)
  auto StdIntrinsicsSadd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUadd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSsub(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUsub(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSmul(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUmul(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSdiv(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUdiv(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSrem(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUrem(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsBitShl(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsBitShr(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsBitIor(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsBitAnd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsBitXor(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsEq(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsOeq(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsNe(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsOne(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSlt(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUlt(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsOlt(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSle(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUle(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsOle(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSgt(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUgt(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsOgt(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSge(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUge(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsOge(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFadd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFsub(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFmul(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFdiv(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFrem(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSaddWrapping(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUaddWrapping(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSsubWrapping(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUsubWrapping(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSmulWrapping(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUmulWrapping(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // BinOp (SimpleIntrinsicBinopAssign)
  auto StdIntrinsicsSaddAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUaddAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSsubAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUsubAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSmulAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUmulAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSdivAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUdivAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSremAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUremAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsBitShlAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsBitShrAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsBitIorAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsBitAndAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsBitXorAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFaddAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFsubAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFmulAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFdivAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFremAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // UnOp (SimpleIntrinsicUnop / SimpleIntrinsicUnopAssign)
  auto StdIntrinsicsSneg(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFneg(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsBitNot(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsBitNotAssign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // ConvOp (SimpleIntrinsicConv)
  auto StdIntrinsicsSitofp(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUitofp(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFptrunc(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsStrunc(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUtrunc(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSzext(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUzext(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFpext(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsBitCast(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFptosi(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFptoui(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // "is this constant" (SimpleIntrinsicIsConst)
  auto StdNumFloatIsZero(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNumFloatIsOne(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNumIntIsZero(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNumIntIsOne(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

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

  // Raw LLVM intrinsic calls, "(T, T) -> T" (SimpleBinaryIntrinsicCall)
  auto StdIntrinsicsSmax(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUmax(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSmin(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUmin(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFpowi(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFpowf(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFatan2(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFmax(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFmin(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFcopysign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSaddSaturating(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUaddSaturating(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSsubSaturating(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUsubSaturating(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSshlSaturating(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUshlSaturating(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // Raw LLVM intrinsic calls, "(T, T) -> (T, Bool)" (SimpleBinaryIntrinsicCallOverflow)
  auto StdIntrinsicsSaddOverflow(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUaddOverflow(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSsubOverflow(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUsubOverflow(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsSmulOverflow(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUmulOverflow(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // Raw LLVM intrinsic calls, "(T) -> T" (SimpleUnaryIntrinsicCall)
  auto StdIntrinsicsAbs(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFsqrt(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFsin(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFcos(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFtan(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFasin(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFacos(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFatan(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFsinh(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFcosh(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFtanh(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFexp(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFexp2(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFexp10(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFlog(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFlog2(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFlog10(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFabs(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFfloor(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFceil(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFtrunc(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsFround(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsBitreverse(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsCtlz(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdDebugBreakpointInternal(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // Three-way integer comparisons (bespoke: two-type-overloaded intrinsic, operand type read off "this")
  auto StdIntrinsicsScmp(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdIntrinsicsUcmp(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // Bespoke: needs a genuinely custom shape (two different argument types + Bool return)
  auto StdIntrinsicsFpclass(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // Bespoke: coroutines / arrays / vectors / slots / futures / memory / atomics (Layer 1, or a Layer 2b helper)
  auto StdArrayIterMov(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdArrayReverseIterMov(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdArrayFwdRef(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdArrayFwdMut(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdVectorFwdRef(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdVectorFwdMut(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto StdGeneratorSend(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto StdGeneratorDrop(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdGeneratorOnceSend(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto StdStringViewSliceRef(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdStringViewSliceMut(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto StdViewIndexRef(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdViewIndexMut(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdViewSliceRef(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdViewSliceMut(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdViewIterRef(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdViewIterMut(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdViewReverseIterRef(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdViewReverseIterMut(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto StdFunctionFunMovDrop(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdCffiCClosureFrom(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNonNullRead(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNonNullWrite(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNonNullRaw(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNonNullEraseType(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNonNullCast(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNonNullFromPtrInner(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNonNullFwdMut(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdNonNullFwdRef(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto StdVolRead(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdVolWrite(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdVolReplace(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto StdRawBufIndexRef(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdRawBufIndexMut(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdRawBufTakeAt(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdRawBufPlaceAt(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdRawBufShift(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdRawBufClearRange(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto StdMemOpsSizeOf(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdMemOpsAlignOf(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdMemOpsSizeOfVal(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdMemOpsAlignOfVal(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdMemOpsReplace(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdMemOpsDrop(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdMemOpsDropInPlace(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto StdThreadingAtomicIsLockFree(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicFenceInner(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicLoadInner(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicStoreInner(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicCompexInner(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicCompexWeakInner(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto StdThreadingAtomicFetchExchange(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicFetchAnd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicFetchNand(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicFetchOr(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicFetchXor(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicFetchNot(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicFetchAdd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicFetchSub(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicFetchFadd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicFetchFsub(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicFetchFmax(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicFetchFmin(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicFetchSmax(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicFetchUmax(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicFetchSmin(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto StdThreadingAtomicFetchUmin(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
}
