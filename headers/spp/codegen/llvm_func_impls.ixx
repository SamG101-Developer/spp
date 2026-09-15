module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_func_impls;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct TypeAst);
use(spp::asts::meta, struct CompilerMetaData);
use(spp::codegen, struct LlvmCtx);

export namespace spp::codegen::func_impls {
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
  auto simple_create_fn(
    SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ret_ty, Vec<llvm::Type*> const &param_tys) -> llvm::Function*;

  /// ======================================================
  /// Layer 2: one builder per repeated function-body "shape",
  /// parameterized by a scoped enum (no closures, no macros,
  /// no templates) identifying which operation to apply.
  /// ======================================================

  /// True if "op" is a comparison, and therefore returns
  /// "Bool" (i1) rather than the operand type.
  auto is_cmp_bin_op(BinOp op) -> bool;

  /// True if "op" is a shift, whose distance operand is
  /// separately typed in the source and so needs coercing.
  auto is_shift_bin_op(BinOp op) -> bool;

  /// Build the actual instruction for a "BinOp" on operands
  /// "a" and "b".
  auto apply_bin_op(LlvmCtx *ctx, BinOp op, llvm::Value *a, llvm::Value *b) -> llvm::Value*;

  /// Build the actual instruction for a "UnOp" on operand "a".
  auto apply_un_op(LlvmCtx *ctx, UnOp op, llvm::Value *a) -> llvm::Value*;

  /// Build the actual instruction for a "ConvOp" converting
  /// "a" to "dest_ty".
  auto apply_conv_op(LlvmCtx *ctx, ConvOp op, llvm::Value *a, llvm::Type *dest_ty) -> llvm::Value*;

  /// "(T, T) -> T" (or "-> Bool" for comparison ops): apply
  /// a "BinOp" to the two incoming arguments and return it.
  auto simple_intrinsic_binop(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, BinOp op) -> void;

  /// "(&mut T, T) -> Void": load the current value out of the
  /// first (pointer) argument, apply a "BinOp" against the
  /// second argument, and store the result back - the "_assign"
  /// (compound-assignment) shape.
  auto simple_intrinsic_binop_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, BinOp op) -> void;

  /// "(T) -> T": apply a "UnOp" to the incoming argument and
  /// return it.
  auto simple_intrinsic_unop(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, UnOp op) -> void;

  /// "(&mut T) -> Void": load the current value out of the
  /// (pointer) argument, apply a "UnOp", and store it back.
  auto simple_intrinsic_unop_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, UnOp op) -> void;

  /// "(Src) -> Dest": apply a "ConvOp" to the incoming
  /// argument. Unlike every other builder here, the return
  /// type is not "ty" - it's derived from "proto"'s own
  /// declared return type, since conversions genuinely go
  /// from one type to a different one (e.g. "S32 -> F64");
  /// "ty" is only the source/operand type.
  auto simple_intrinsic_conv(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, ConvOp op) -> void;

  /// "(T) -> Bool": compare the incoming argument for
  /// equality against a fixed constant (e.g. "is_zero"/
  /// "is_one").
  auto simple_intrinsic_is_const(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, bool is_float, double value) -> void;

  /// Map an "AtomicRmwOp" to the underlying
  /// "llvm::AtomicRMWInst::BinOp". Todo: Just use
  /// the original enum?
  auto apply_atomic_rmw_op(AtomicRmwOp op) -> llvm::AtomicRMWInst::BinOp;

  /// "(&self, val: T, order: U8) -> T": The atomic methods
  /// "Atom[T]::fetch_*"/"exchange" all share this exact
  /// shape - atomically apply an "AtomicRmwOp" between
  /// "self.val" and "val", returning "self.val"'s value
  /// from before the operation (which is exactly what
  /// "llvm.atomicrmw" itself returns, so no extra load/
  /// store logic is needed).
  auto simple_atomic_fetch_rmw(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, AtomicRmwOp op) -> void;

  /// "(T, T) -> T": call a two-operand LLVM intrinsic directly
  /// (e.g. "llvm.smax") and return its result as-is - for
  /// intrinsics whose result type genuinely is "T" (unlike,
  /// say, the "with.overflow" family below).
  auto simple_binary_intrinsic_call(
    SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void;

  /// "(T, T) -> (T, Bool)": call a two-operand "with.overflow"-
  /// shaped LLVM intrinsic (e.g. "llvm.sadd.with.overflow"),
  /// whose result is already the literal struct "{T, i1}" that
  /// "(T, Bool)" lowers to, so it's returned as-is.
  auto simple_binary_intrinsic_call_overflow(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty,
    llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void;

  /// "(T) -> T": call a one-operand LLVM intrinsic directly
  /// (e.g. "llvm.sqrt") and return its result.
  auto simple_unary_intrinsic_call(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty,
    llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void;

  /// "(T) -> T", ignoring the argument entirely: always return
  /// the given (already-computed) constant value. Used for
  /// every "zero-arg" builtin (neg_one/zero/one/two/min_val/
  /// max_val/...) - S++ still synthesizes a dummy one-argument
  /// signature for these regardless of true arity, matching
  /// every other builder here.
  auto simple_get_value(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, llvm::Value *val) -> void;

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
  auto simple_coro_iter(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, bool reverse, bool borrow) -> void;

  /// Shared codegen for a coroutine that hands out the elements
  /// of a dynamically-sized view one at a time, using a loop
  /// and counter strategy.
  auto simple_coro_view_iter(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, bool reverse, bool borrow) -> void;

  /// Shared logic for the two NonNull forwarding calls, to &T
  /// and &mut T - in S++ they are different but in LLVM it's the
  /// same pointer technique: Todo: different attributes/flags?
  auto simple_coro_non_null_fwd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx) -> void;

  /// Shared forwarding for providing a forwarding call to a
  /// View of a collection, like a vector or array. Again, the
  /// two forwarding calls for any collections will share this.
  /// This is the core of the following two functions.
  auto simple_coro_contiguous_fwd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Value *data, llvm::Value *length) -> void;

  /// The array wrapper of the above lowering codegen for the
  /// contiguous view forwarding.
  auto simple_coro_array_fwd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx) -> void;

  /// The vector wrapper of the above lowering codegen for the
  /// contiguous view forwarding.
  auto simple_coro_vector_fwd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx) -> void;

  /// Shared codegen for indexing a view, based on an index. It
  /// does the (provably-safe) pointer math and GEPs the element
  /// from the pointer/length.
  auto simple_coro_view_index(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx) -> void;

  /// Shared codegen for slicing a view, based on two bounds. It
  /// does the (provably-safe) pointer math and GEPs the slice from
  /// the pointer/length.
  auto simple_coro_view_slice(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx) -> void;

  /// ======================================================
  /// Layer 3: individual builtin implementations, grouped by
  /// which Layer 2 builder (if any) they use.
  /// ======================================================

  // BinOp (simple_intrinsic_binop)
  auto std_intrinsics_sadd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_uadd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ssub(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_usub(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_smul(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_umul(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_sdiv(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_udiv(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_srem(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_urem(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_shl(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_shr(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_ior(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_and(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_xor(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_eq(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_oeq(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ne(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_one(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_slt(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ult(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_olt(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_sle(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ule(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ole(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_sgt(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ugt(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ogt(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_sge(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_uge(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_oge(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fadd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fsub(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fmul(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fdiv(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_frem(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_sadd_wrapping(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_uadd_wrapping(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ssub_wrapping(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_usub_wrapping(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_smul_wrapping(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_umul_wrapping(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // BinOp (simple_intrinsic_binop_assign)
  auto std_intrinsics_sadd_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_uadd_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ssub_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_usub_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_smul_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_umul_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_sdiv_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_udiv_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_srem_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_urem_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_shl_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_shr_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_ior_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_and_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_xor_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fadd_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fsub_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fmul_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fdiv_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_frem_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // UnOp (simple_intrinsic_unop / simple_intrinsic_unop_assign)
  auto std_intrinsics_sneg(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fneg(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_not(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_not_assign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // ConvOp (simple_intrinsic_conv)
  auto std_intrinsics_sitofp(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_uitofp(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fptrunc(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_strunc(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_utrunc(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_szext(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_uzext(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fpext(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_cast(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fptosi(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fptoui(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // "is this constant" (simple_intrinsic_is_const)
  auto std_num_float_is_zero(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_float_is_one(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_int_is_zero(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_int_is_one(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // Fixed values (simple_get_value)
  auto std_array_new(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_float_neg_one(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_float_zero(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_float_one(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_int_neg_one(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_int_zero(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_int_one(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_int_two(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_min_val(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_max_val(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fmin_val(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fmax_val(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // Raw LLVM intrinsic calls, "(T, T) -> T" (simple_binary_intrinsic_call)
  auto std_intrinsics_smax(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_umax(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_smin(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_umin(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fpowi(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fpowf(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fatan2(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fmax(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fmin(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fcopysign(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_sadd_saturating(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_uadd_saturating(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ssub_saturating(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_usub_saturating(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_sshl_saturating(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ushl_saturating(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // Raw LLVM intrinsic calls, "(T, T) -> (T, Bool)" (simple_binary_intrinsic_call_overflow)
  auto std_intrinsics_sadd_overflow(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_uadd_overflow(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ssub_overflow(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_usub_overflow(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_smul_overflow(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_umul_overflow(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // Raw LLVM intrinsic calls, "(T) -> T" (simple_unary_intrinsic_call)
  auto std_intrinsics_abs(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fsqrt(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fsin(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fcos(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ftan(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fasin(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_facos(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fatan(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fsinh(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fcosh(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ftanh(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fexp(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fexp2(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fexp10(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_flog(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_flog2(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_flog10(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fabs(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ffloor(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fceil(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ftrunc(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fround(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bitreverse(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ctlz(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_debug_breakpoint_internal(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // Three-way integer comparisons (bespoke: two-type-overloaded intrinsic, operand type read off "this")
  auto std_intrinsics_scmp(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ucmp(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // Bespoke: needs a genuinely custom shape (two different argument types + Bool return)
  auto std_intrinsics_fpclass(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  // Bespoke: coroutines / arrays / vectors / slots / futures / memory / atomics (Layer 1, or a Layer 2b helper)
  auto std_array_iter_mov(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_array_reverse_iter_mov(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_array_fwd_ref(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_array_fwd_mut(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_vector_fwd_ref(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_vector_fwd_mut(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_generator_send(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_generator_drop(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_generator_once_send(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_string_view_slice_ref(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_string_view_slice_mut(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_view_index_ref(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_view_index_mut(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_view_slice_ref(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_view_slice_mut(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_view_iter_ref(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_view_iter_mut(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_view_reverse_iter_ref(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_view_reverse_iter_mut(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_function_fun_mov_drop(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_cffi_c_closure_from(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_non_null_read(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_non_null_write(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_non_null_raw(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_non_null_erase_type(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_non_null_cast(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_non_null_from_ptr_inner(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_non_null_fwd_mut(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_non_null_fwd_ref(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_vol_read(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_vol_write(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_vol_replace(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_raw_buf_index_ref(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_raw_buf_index_mut(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_raw_buf_take_at(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_raw_buf_place_at(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_raw_buf_shift(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_raw_buf_clear_range(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_mem_ops_size_of(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_mem_ops_align_of(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_mem_ops_size_of_val(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_mem_ops_align_of_val(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_mem_ops_replace(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_mem_ops_drop(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_mem_ops_drop_in_place(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_threading_atomic_is_lock_free(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fence_inner(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_load_inner(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_store_inner(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_compex_inner(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_compex_weak_inner(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_threading_atomic_fetch_exchange(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_and(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_nand(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_or(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_xor(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_not(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_add(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_sub(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_fadd(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_fsub(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_fmax(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_fmin(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_smax(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_umax(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_smin(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_umin(SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void;
}
