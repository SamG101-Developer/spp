module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_func_impls;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS class ScopeManager;
  SPP_EXP_CLS struct VariableSymbol;
}

namespace spp::asts {
  SPP_EXP_CLS struct FunctionPrototypeAst;
  SPP_EXP_CLS struct TypeAst;
}

namespace spp::asts::meta {
  SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::codegen {
  SPP_EXP_CLS struct LLvmCtx;
}

/**
 * This namespace provides implementations of functions whose implementations are so low that they cannot be expressed
 * in safe S++. The code presented in LLVM IR is all safe, manually checked rather than via S++ borrow checker or other
 * semantic checks. It is akin to manually written assembly code and checking every instruction for safety, for use with
 * C. These function implementations are grabbed from @c@compiler_builtin tagged functions/methods. Once a tag is found,
 * the function name is used to locate the implementation in this namespace.
 *
 * The LLVM IR returned from these functions is in string form, and is injected into the LLVM module by the codegen
 * stage.
 *
 * The helpers here are organized in three layers:
 *  - Layer 1 (@c simple_create_fn): the one place that creates the mangled "llvm::Function" and its entry block.
 *    Every other layer eventually bottoms out here.
 *  - Layer 2 (the "simple_*" builders below the enums, plus @c simple_binary_intrinsic_call /
 *    @c simple_unary_intrinsic_call / @c simple_get_value): each captures one repeated *shape* of function body
 *    (binary op, unary op, conversion, "is this constant", raw LLVM intrinsic call, or "return a fixed value"), and
 *    is driven by a small scoped enum rather than a closure, so there is exactly one place that knows how to build
 *    each shape.
 *  - Layer 3 (everything prefixed @c std_): the individual builtin implementations. Most are one- or two-line calls
 *    into a Layer 2 builder; a handful of genuinely bespoke ones (coroutines, atomics, memory placement, Fut::await)
 *    call Layer 1 directly because their body doesn't fit any of the Layer 2 shapes.
 */
export namespace spp::codegen::func_impls {
  // =====================================================================================================
  // Layer 1: function + entry-block creation. The root every other layer is built on.
  // =====================================================================================================

  /**
   * Create the mangled "llvm::Function" for "proto" with the given signature, plus its entry block, and leave the
   * builder's insert point set there. Factors out the "name + FunctionType + Function::Create + entry BasicBlock"
   * boilerplate that would otherwise be repeated at the top of every hand-written func_impls body.
   * @param ctx The LLVM context.
   * @param ret_ty The function's return type.
   * @param param_tys The function's parameter types, in order.
   * @return The newly created (empty) function, with its entry block as the current insert point.
   */
  auto simple_create_fn(
    SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ret_ty, Vec<llvm::Type*> const &param_tys) -> llvm::Function*;

  // =====================================================================================================
  // Layer 2: one builder per repeated function-body "shape", parameterized by a scoped enum (no closures, no
  // macros, no templates) identifying which operation to apply.
  // =====================================================================================================

  /** Binary arithmetic/bitwise/comparison operations shareable across "(T, T) -> T" (or "-> Bool" for comparisons). */
  enum class BinOp {
    Add, Sub, Mul, SDiv, UDiv, SRem, URem, Shl, LShr, Or, And, Xor,
    ICmpEQ, ICmpNE, ICmpSLT, ICmpULT, ICmpSLE, ICmpULE, ICmpSGT, ICmpUGT, ICmpSGE, ICmpUGE,
    FCmpOEQ, FCmpONE, FCmpOLT, FCmpOLE, FCmpOGT, FCmpOGE,
    FAdd, FSub, FMul, FDiv, FRem,
    NSWAdd, NUWAdd, NSWSub, NUWSub, NSWMul, NUWMul,
    LogicalAnd, LogicalOr,
  };

  /** Unary arithmetic operations shareable across "(T) -> T". */
  enum class UnOp { Neg, Not, FNeg };

  /** Value-conversion operations: "(Src) -> Dest", where (unlike every other shape here) Src and Dest can differ. */
  enum class ConvOp { SIToFP, UIToFP, FPTrunc, Trunc, SExt, ZExt, FPExt, BitCast, FPToSI, FPToUI };

  /** True if "op" is a comparison, and therefore returns "Bool" (i1) rather than the operand type. */
  auto is_cmp_bin_op(BinOp op) -> bool;

  /** Build the actual instruction for a "BinOp" on operands "a" and "b". */
  auto apply_bin_op(LLvmCtx *ctx, BinOp op, llvm::Value *a, llvm::Value *b) -> llvm::Value*;

  /** Build the actual instruction for a "UnOp" on operand "a". */
  auto apply_un_op(LLvmCtx *ctx, UnOp op, llvm::Value *a) -> llvm::Value*;

  /** Build the actual instruction for a "ConvOp" converting "a" to "dest_ty". */
  auto apply_conv_op(LLvmCtx *ctx, ConvOp op, llvm::Value *a, llvm::Type *dest_ty) -> llvm::Value*;

  /**
   * "(T, T) -> T" (or "-> Bool" for comparison ops): apply a "BinOp" to the two incoming arguments and return it.
   * @param ty The operand type "T".
   */
  auto simple_intrinsic_binop(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, BinOp op) -> void;

  /**
   * "(&mut T, T) -> Void": load the current value out of the first (pointer) argument, apply a "BinOp" against the
   * second argument, and store the result back - the "_assign" (compound-assignment) shape.
   * @param ty The operand type "T".
   */
  auto simple_intrinsic_binop_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, BinOp op) -> void;

  /**
   * "(T) -> T": apply a "UnOp" to the incoming argument and return it.
   * @param ty The operand type "T".
   */
  auto simple_intrinsic_unop(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, UnOp op) -> void;

  /**
   * "(&mut T) -> Void": load the current value out of the (pointer) argument, apply a "UnOp", and store it back.
   * @param ty The operand type "T".
   */
  auto simple_intrinsic_unop_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, UnOp op) -> void;

  /**
   * "(Src) -> Dest": apply a "ConvOp" to the incoming argument. Unlike every other builder here, the return type is
   * not "ty" - it's derived from "proto"'s own declared return type, since conversions genuinely go from one type to
   * a different one (e.g. "S32 -> F64"); "ty" is only the source/operand type.
   * @param ty The operand's (source) type.
   */
  auto simple_intrinsic_conv(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, ConvOp op) -> void;

  /**
   * "(T) -> Bool": compare the incoming argument for equality against a fixed constant (e.g. "is_zero"/"is_one").
   * @param ty The operand type "T".
   * @param is_float Whether to build the constant/comparison as a float ("FCmpOEQ") or integer ("ICmpEQ").
   * @param value The constant to compare against.
   */
  auto simple_intrinsic_is_const(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, bool is_float, double value) -> void;

  /** Atomic read-modify-write operations shareable across "Atom[T]::fetch_*"/"exchange". */
  enum class AtomicRmwOp { Xchg, Add, Sub, And, Nand, Or, Xor, Max, Min, UMax, UMin, FAdd, FSub, FMax, FMin };

  /** Map an "AtomicRmwOp" to the underlying "llvm::AtomicRMWInst::BinOp". */
  auto apply_atomic_rmw_op(AtomicRmwOp op) -> llvm::AtomicRMWInst::BinOp;

  /**
   * "(&self, val: T, order: U8) -> T": "Atom[T]::fetch_*"/"exchange" all share this exact shape - atomically apply
   * an "AtomicRmwOp" between "self.val" and "val", returning "self.val"'s value from *before* the operation (which
   * is exactly what "llvm.atomicrmw" itself returns, so no extra load/store choreography is needed). These are
   * methods (not coroutines, and not free "_inner" functions), so - like "std_slot_replace" - this builds directly
   * into the already-declared/open function rather than via "simple_create_fn".
   */
  auto simple_atomic_fetch_rmw(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, AtomicRmwOp op) -> void;

  /**
   * "(T, T) -> T": call a two-operand LLVM intrinsic directly (e.g. "llvm.smax") and return its result as-is - for
   * intrinsics whose result type genuinely is "T" (unlike, say, the "with.overflow" family below).
   */
  auto simple_binary_intrinsic_call(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty,
    llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void;

  /**
   * "(T, T) -> (T, Bool)": call a two-operand "with.overflow"-shaped LLVM intrinsic (e.g. "llvm.sadd.with.overflow"),
   * whose result is already the literal struct "{T, i1}" that "(T, Bool)" lowers to, so it's returned as-is.
   */
  auto simple_binary_intrinsic_call_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty,
    llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void;

  /**
   * "(T) -> T": call a one-operand LLVM intrinsic directly (e.g. "llvm.sqrt") and return its result.
   */
  auto simple_unary_intrinsic_call(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty,
    llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void;

  /**
   * "(T) -> T", ignoring the argument entirely: always return the given (already-computed) constant value. Used for
   * every "zero-arg" builtin (neg_one/zero/one/two/min_val/max_val/...) - S++ still synthesizes a dummy one-argument
   * signature for these regardless of true arity, matching every other builder here.
   */
  auto simple_get_value(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, llvm::Value *val) -> void;

  // =====================================================================================================
  // Layer 2b: coroutine-specific shared helpers (still "one shape, many callers", just not enum-driven since each
  // is only reused by a forwards/backwards pair rather than a whole family of operations).
  // =====================================================================================================

  /**
   * Shared codegen for a coroutine that hands out the elements of a fixed-size, inline array one at a time, moving
   * each element out via a "gen"-style suspend/resume point. The array's length is a compile-time constant, so this
   * unrolls into one yield per element (forwards or backwards) instead of a runtime loop, matching exactly what a
   * hand-written "gen self[i]" loop would lower to. Used by @c std_array_iter_mov and @c std_array_reverse_iter_mov;
   * a future "View" iterator (over a runtime-length pointer/length pair) will need a different, loop-based helper
   * since its length is not known at compile time.
   * @param proto The coroutine prototype (must be a @c CoroutinePrototypeAst); its env/resume function must already
   * be built by the time this runs (true when called from @c FunctionImplementationLoweredAst::Stage11_CodeGen).
   * @param ctx The llvm context.
   * @param reverse If true, yields from the last element to the first; otherwise first to last.
   */
  auto simple_coro_iter(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, bool reverse, bool borrow) -> void;

  /**
   * Shared codegen for "NonNull[T]::fwd_ref"/"fwd_mut": "(&self) -> GenOnce[&T]" (or "&mut T"). "NonNull[T]" lowers to
   * a bare "ptr" (see "kNonNullParts" in llvm_type.cpp), so unlike "Slot[T]::get_ref"/"get_mut" there is no struct
   * field to "GEP" into - the address to yield is simply the pointer value "self" itself wraps, read out with a
   * second load past the borrow's own indirection. Otherwise identical in shape to "simple_coro_slot_get": a single
   * "gen"-style suspend/resume, matching "GenOnce".
   */
  auto simple_coro_non_null_fwd(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void;

  /**
   * Shared codegen for "StrView::slice_ref"/"slice_mut": "(&self, from: USize, into: USize) -> GenOnce[&StrView]"
   * (or "&mut StrView"). Computes a new "StrView { ptr: self.ptr + from, length: into - from }" - a *view* over
   * "self", not a new owned object - and yields a reference to it, suspending once (matching "GenOnce").
   *
   * Unlike "Slot"'s borrow target, this new "{ptr, length}" pair doesn't already exist anywhere - it has to be
   * materialized fresh, and that storage must survive past this suspend point. Rather than growing the coroutine's
   * env struct with a new field (as "Arr"'s "View" forwarding would need), this reuses the "from"/"into" parameters'
   * own frame slots: both are "USize" (8 bytes), declared back-to-back, so together they are exactly a 16-byte,
   * appropriately-aligned region - the same shape as "StrView". Their original values are read out first (the only
   * thing they're ever used for), then that same memory is overwritten with the freshly computed "{ptr, length}"
   * and yielded by address - safe because "GenOnce" only ever yields once, so neither slot is read again afterward.
   */
  auto simple_coro_view_slice(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void;

  /**
   * Shared codegen for "View[T]::index_ref"/"index_mut": "(&self, index: USize) -> Indexed[&T]" (or "&mut T").
   * "Indexed[T]" is just "GenOnce[T]" under a clearer name. Bounds-checks "index" against "self.length"; if out of
   * bounds, traps immediately (matching the "will abort if the index is out of bounds" contract - no message, since
   * there's no S++-level string to print from here). Otherwise computes "self.ptr + index" and yields it, suspending
   * once. No scratch storage needed - the yielded address already lives inside "self"'s own buffer.
   */
  auto simple_coro_view_index(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void;

  // =====================================================================================================
  // Layer 3: individual builtin implementations, grouped by which Layer 2 builder (if any) they use.
  // =====================================================================================================

  // --- BinOp (simple_intrinsic_binop) ---
  auto std_boolean_and(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_boolean_ior(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_add(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_sub(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_mul(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_sdiv(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_udiv(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_srem(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_urem(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_shl(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_shr(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_ior(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_and(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_xor(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_eq(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_oeq(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ne(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_one(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_slt(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ult(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_olt(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_sle(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ule(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ole(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_sgt(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ugt(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ogt(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_sge(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_uge(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_oge(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fadd(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fsub(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fmul(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fdiv(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_frem(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_sadd_wrapping(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_uadd_wrapping(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ssub_wrapping(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_usub_wrapping(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_smul_wrapping(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_umul_wrapping(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  // --- BinOp (simple_intrinsic_binop_assign) ---
  auto std_intrinsics_add_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_sub_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_mul_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_sdiv_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_udiv_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_srem_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_urem_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_shl_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_shr_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_ior_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_and_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_xor_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fadd_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fsub_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fmul_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fdiv_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_frem_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  // --- UnOp (simple_intrinsic_unop / simple_intrinsic_unop_assign) ---
  auto std_intrinsics_sneg(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fneg(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_not(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_not_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  // --- ConvOp (simple_intrinsic_conv) ---
  auto std_intrinsics_sitofp(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_uitofp(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fptrunc(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_strunc(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_utrunc(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_szext(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_uzext(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fpext(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bit_cast(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fptosi(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fptoui(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  // --- "is this constant" (simple_intrinsic_is_const) ---
  auto std_num_float_is_zero(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_float_is_one(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_int_is_zero(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_int_is_one(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  // --- Fixed values (simple_get_value) ---
  auto std_array_new(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_float_neg_one(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_float_zero(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_float_one(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_int_neg_one(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_int_zero(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_int_one(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_num_int_two(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_min_val(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_max_val(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fmin_val(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fmax_val(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  // --- Raw LLVM intrinsic calls, "(T, T) -> T" (simple_binary_intrinsic_call) ---
  auto std_intrinsics_smax(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_umax(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_smin(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_umin(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fpowi(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fpowf(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fatan2(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fmax(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fmin(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fcopysign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_sadd_saturating(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_uadd_saturating(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ssub_saturating(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_usub_saturating(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_sshl_saturating(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ushl_saturating(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  // --- Raw LLVM intrinsic calls, "(T, T) -> (T, Bool)" (simple_binary_intrinsic_call_overflow) ---
  auto std_intrinsics_sadd_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_uadd_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ssub_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_usub_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_smul_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_umul_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  // --- Raw LLVM intrinsic calls, "(T) -> T" (simple_unary_intrinsic_call) ---
  auto std_intrinsics_abs(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fsqrt(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fsin(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fcos(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ftan(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fasin(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_facos(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fatan(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fsinh(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fcosh(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ftanh(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fexp(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fexp2(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fexp10(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_flog(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_flog2(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_flog10(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fabs(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ffloor(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fceil(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ftrunc(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_fround(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_bitreverse(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ctlz(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_debug_breakpoint_internal(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  // --- Three-way integer comparisons (bespoke: two-type-overloaded intrinsic, operand type read off "this") ---
  auto std_intrinsics_scmp(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_intrinsics_ucmp(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  // --- Bespoke: needs a genuinely custom shape (two different argument types + Bool return) ---
  auto std_intrinsics_fpclass(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  // --- Bespoke: coroutines / arrays / vectors / slots / futures / memory / atomics (Layer 1, or a Layer 2b helper) ---
  auto std_array_iter_mov(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_array_reverse_iter_mov(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_array_fwd_ref(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_array_fwd_mut(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_vector_fwd_ref(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_vector_fwd_mut(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_generator_send(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_generator_once_send(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_slot_get_ref(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_slot_get_mut(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_slot_replace(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_string_view_slice_ref(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_string_view_slice_mut(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_view_index_ref(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_view_index_mut(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_view_slice_ref(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_view_slice_mut(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_view_iter_ref(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_view_iter_mut(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_view_reverse_iter_ref(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_view_reverse_iter_mut(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_non_null_read(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_non_null_write(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_non_null_raw(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_non_null_cast(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_non_null_from_ptr_inner(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_non_null_fwd_mut(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_non_null_fwd_ref(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_vol_read(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_vol_write(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_vol_replace(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_raw_buf_index_ref(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_raw_buf_index_mut(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_raw_buf_take_at(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_raw_buf_place_at(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_raw_buf_shift(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  /**
   * Todo: stubbed as a no-op. "RawBuf::clear_range" is documented to run each element's destructor in place, but the
   * compiler has no destructor-dispatch codegen yet anywhere (no scope-exit drops, and "std.mem.ops.drop_in_place" -
   * the primitive this would be built from - is itself still an unregistered intrinsic). Once that exists, this
   * should loop "start..start+count" calling it once per element.
   */
  auto std_raw_buf_clear_range(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_mem_ops_size_of(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_mem_ops_align_of(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_mem_ops_size_of_val(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_mem_ops_align_of_val(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_mem_ops_replace(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  /**
   * Todo: stubbed as a no-op, same blocker as "std_raw_buf_clear_range" - the compiler has no destructor-dispatch
   * codegen anywhere yet, and "drop_in_place" is exactly the primitive that would need it. Once that exists, this
   * should run "T"'s destructor on the value behind "ptr" in place.
   */
  auto std_mem_ops_drop_in_place(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_threading_atomic_is_lock_free(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_threading_atomic_fence_inner(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_load_inner(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_store_inner(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_compex_inner(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_compex_weak_inner(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

  auto std_threading_atomic_fetch_exchange(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_and(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_nand(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_or(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_xor(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_not(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_add(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_sub(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_fadd(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_fsub(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_fmax(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_fmin(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_smax(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_umax(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_smin(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
  auto std_threading_atomic_fetch_umin(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
}
