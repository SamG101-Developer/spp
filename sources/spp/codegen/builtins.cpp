module;
#include <spp/macros.hpp>

module spp.codegen.builtins;
import spp.analyse.utils.comp_time_intrinsics;
import spp.asts.boolean_literal_ast;
import spp.asts.float_literal_ast;
import spp.asts.integer_literal_ast;
import spp.codegen.llvm_fn_impls;

MSVC_DEVCOM_11179619_IMPL_UNIT_USING_DECL
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::asts, FunctionPrototypeAst);
use_alias(spp::asts::meta, CompilerMetaData);

namespace spp::codegen::builtins {
  namespace ct = analyse::utils::comp_time_intrinsics;
  using fn_impls::AtomicRmwOp;
  using fn_impls::FloatMathFn;
  using fn_impls::BinOp;
  using fn_impls::ConvOp;
  using fn_impls::UnOp;
  using LlvmImplFn = decltype(LoweredFnImpl::LlvmImpl);

  /// File the builtin "name": how it is lowered, how it is computed at compile time if it can be, and the generic
  /// parameter whose destructor its lowering calls, if any ("T" for "drop"), and how a direct call to it is lowered
  /// in place, if it is (a branch hint).
  static auto Register(
    Map<Str, LoweredFnImpl> &map, Str const &name, LlvmImplFn llvm_impl,
    Unique<ct::CompTimeFn> cmp_impl = nullptr, Str drops_gn = "",
    decltype(LoweredFnImpl::CallSiteImpl) call_site_impl = nullptr)
    -> void;

  // The lowerings that are one call to a shared builder ("fn_impls::Simple*"), bound to what picks the operation:
  // an operator, an LLVM intrinsic, a direction. A builtin that is anything more keeps a function of its own.
  static auto Binop(const BinOp op) -> LlvmImplFn;

  static auto BinopAssign(const BinOp op) -> LlvmImplFn;

  static auto Unop(const UnOp op) -> LlvmImplFn;

  static auto UnopAssign(const UnOp op) -> LlvmImplFn;

  static auto Conv(const ConvOp op) -> LlvmImplFn;

  static auto IsConst(const bool is_float, const double value) -> LlvmImplFn;

  static auto BinaryIntrinsic(const llvm::Intrinsic::IndependentIntrinsics intrinsic) -> LlvmImplFn;

  static auto OverflowIntrinsic(const llvm::Intrinsic::IndependentIntrinsics intrinsic) -> LlvmImplFn;

  /// "op" ("FAdd", ...) with whether it raised a float exception; "unary" for the square root.
  static auto FloatRaised(const BinOp op, const bool unary) -> LlvmImplFn;

  static auto UnaryIntrinsic(const llvm::Intrinsic::IndependentIntrinsics intrinsic) -> LlvmImplFn;

  /// A maths function, checked as float arithmetic is ("*_ieee" is the plain intrinsic).
  static auto FloatMath(const FloatMathFn math) -> LlvmImplFn;

  static auto BitCount(const llvm::Intrinsic::IndependentIntrinsics intrinsic) -> LlvmImplFn;

  static auto FunnelShift(const llvm::Intrinsic::IndependentIntrinsics intrinsic) -> LlvmImplFn;

  static auto AtomicRmw(const AtomicRmwOp op) -> LlvmImplFn;

  static auto CoroIter(const bool reverse) -> LlvmImplFn;

  static auto CoroViewIter(const bool reverse, const bool borrow) -> LlvmImplFn;

  /// A builder that reads everything it needs off the prototype (a "&" and a
  /// "&mut" form lower alike), so it takes no return type.
  static auto Untyped(
    void (*build)(ScopeManager *, FunctionPrototypeAst const *, CompilerMetaData *, LlvmCtx *))
    -> LlvmImplFn;

  /// A branch hint, lowered at the call site to the condition expected to be
  /// "expected".
  static auto Expect(const bool expected) -> decltype(LoweredFnImpl::CallSiteImpl);
}

auto spp::codegen::builtins::Register(
  Map<Str, LoweredFnImpl> &map, Str const &name, LlvmImplFn llvm_impl,
  Unique<ct::CompTimeFn> cmp_impl, Str drops_gn,
  decltype(LoweredFnImpl::CallSiteImpl) call_site_impl)
  -> void {
  map.emplace(name, LoweredFnImpl{
    .LlvmImpl = std::move(llvm_impl), .CompTimeImpl = std::move(cmp_impl),
    .Name = name, .DropsGn = std::move(drops_gn),
    .CallSiteImpl = std::move(call_site_impl)});
}

auto spp::codegen::builtins::Binop(
  const BinOp op) -> LlvmImplFn {
  return [op](auto sm, auto proto, auto meta, auto ctx, auto ty) {
    fn_impls::SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, op);
  };
}

auto spp::codegen::builtins::BinopAssign(
  const BinOp op) -> LlvmImplFn {
  return [op](auto sm, auto proto, auto meta, auto ctx, auto ty) {
    fn_impls::SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, op);
  };
}

auto spp::codegen::builtins::Unop(
  const UnOp op) -> LlvmImplFn {
  return [op](auto sm, auto proto, auto meta, auto ctx, auto ty) {
    fn_impls::SimpleIntrinsicUnop(sm, proto, meta, ctx, ty, op);
  };
}

auto spp::codegen::builtins::UnopAssign(
  const UnOp op) -> LlvmImplFn {
  return [op](auto sm, auto proto, auto meta, auto ctx, auto ty) {
    fn_impls::SimpleIntrinsicUnopAssign(sm, proto, meta, ctx, ty, op);
  };
}

auto spp::codegen::builtins::Conv(
  const ConvOp op) -> LlvmImplFn {
  return [op](auto sm, auto proto, auto meta, auto ctx, auto ty) {
    fn_impls::SimpleIntrinsicConv(sm, proto, meta, ctx, ty, op);
  };
}

auto spp::codegen::builtins::IsConst(
  const bool is_float, const double value) -> LlvmImplFn {
  return [is_float, value](auto sm, auto proto, auto meta, auto ctx, auto ty) {
    fn_impls::SimpleIntrinsicIsConst(sm, proto, meta, ctx, ty, is_float, value);
  };
}

auto spp::codegen::builtins::BinaryIntrinsic(
  const llvm::Intrinsic::IndependentIntrinsics intrinsic) -> LlvmImplFn {
  return [intrinsic](auto sm, auto proto, auto meta, auto ctx, auto ty) {
    fn_impls::SimpleBinaryIntrinsicCall(sm, proto, meta, ctx, ty, intrinsic);
  };
}

auto spp::codegen::builtins::OverflowIntrinsic(
  const llvm::Intrinsic::IndependentIntrinsics intrinsic) -> LlvmImplFn {
  return [intrinsic](auto sm, auto proto, auto meta, auto ctx, auto ty) {
    fn_impls::SimpleBinaryIntrinsicCallOverflow(sm, proto, meta, ctx, ty, intrinsic);
  };
}

auto spp::codegen::builtins::FloatRaised(
  const BinOp op, const bool unary) -> LlvmImplFn {
  return [op, unary](auto sm, auto proto, auto meta, auto ctx, auto ty) {
    fn_impls::SimpleFloatRaised(sm, proto, meta, ctx, ty, op, unary);
  };
}

auto spp::codegen::builtins::FloatMath(
  const FloatMathFn math) -> LlvmImplFn {
  return [math](auto sm, auto proto, auto meta, auto ctx, auto ty) {
    fn_impls::SimpleCheckedFloatMath(sm, proto, meta, ctx, ty, math);
  };
}

auto spp::codegen::builtins::UnaryIntrinsic(
  const llvm::Intrinsic::IndependentIntrinsics intrinsic) -> LlvmImplFn {
  return [intrinsic](auto sm, auto proto, auto meta, auto ctx, auto ty) {
    fn_impls::SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, intrinsic);
  };
}

auto spp::codegen::builtins::BitCount(
  const llvm::Intrinsic::IndependentIntrinsics intrinsic) -> LlvmImplFn {
  return [intrinsic](auto sm, auto proto, auto meta, auto ctx, auto ty) {
    fn_impls::SimpleBitCount(sm, proto, meta, ctx, ty, intrinsic);
  };
}

auto spp::codegen::builtins::FunnelShift(
  const llvm::Intrinsic::IndependentIntrinsics intrinsic) -> LlvmImplFn {
  return [intrinsic](auto sm, auto proto, auto meta, auto ctx, auto ty) {
    fn_impls::SimpleFunnelShift(sm, proto, meta, ctx, ty, intrinsic);
  };
}

auto spp::codegen::builtins::AtomicRmw(
  const AtomicRmwOp op) -> LlvmImplFn {
  return [op](auto sm, auto proto, auto meta, auto ctx, auto) {
    fn_impls::SimpleAtomicFetchRmw(sm, proto, meta, ctx, op);
  };
}

auto spp::codegen::builtins::CoroIter(
  const bool reverse) -> LlvmImplFn {
  return [reverse](auto sm, auto proto, auto meta, auto ctx, auto) {
    fn_impls::SimpleCoroIter(sm, proto, meta, ctx, reverse);
  };
}

auto spp::codegen::builtins::CoroViewIter(
  const bool reverse, const bool borrow) -> LlvmImplFn {
  return [reverse, borrow](auto sm, auto proto, auto meta, auto ctx, auto) {
    fn_impls::SimpleCoroViewIter(sm, proto, meta, ctx, reverse, borrow);
  };
}

auto spp::codegen::builtins::Untyped(
  void (*build)(ScopeManager *, FunctionPrototypeAst const *, CompilerMetaData *, LlvmCtx *))
  -> LlvmImplFn {
  return [build](auto sm, auto proto, auto meta, auto ctx, auto) { build(sm, proto, meta, ctx); };
}

auto spp::codegen::builtins::Expect(
  const bool expected) -> decltype(LoweredFnImpl::CallSiteImpl) {
  return [expected](auto const &args, auto ctx) {
    return fn_impls::StdHintsExpect(args, ctx, expected);
  };
}

auto spp::codegen::builtins::MakeBuiltinFnMap() -> Map<Str, LoweredFnImpl> {
  auto map = Map<Str, LoweredFnImpl>{};

  Register(map, "std.array.Arr.new", fn_impls::StdArrayNew);
  Register(map, "std.array.Arr.iter_mov", CoroIter(false));
  Register(map, "std.array.Arr.reverse_iter_mov", CoroIter(true));
  Register(map, "std.array.Arr.fwd_ref", Untyped(fn_impls::SimpleCoroArrayFwd));
  Register(map, "std.array.Arr.fwd_mut", Untyped(fn_impls::SimpleCoroArrayFwd));

  Register(map, "std.vector.Vec.fwd_ref", Untyped(fn_impls::SimpleCoroVectorFwd));
  Register(map, "std.vector.Vec.fwd_mut", Untyped(fn_impls::SimpleCoroVectorFwd));

  Register(map, "std.generator.Gen.send", fn_impls::StdGeneratorSend);
  Register(map, "std.generator.Gen.drop", fn_impls::StdGeneratorDrop);
  Register(map, "std.generator.GenOnce.drop", fn_impls::StdGeneratorDrop);
  Register(map, "std.generator.GenOnce.send", fn_impls::StdGeneratorOnceSend);

  Register(map, "std.string_view.StrView.slice_ref", Untyped(fn_impls::SimpleCoroViewSlice));
  Register(map, "std.string_view.StrView.slice_mut", Untyped(fn_impls::SimpleCoroViewSlice));

  Register(map, "std.view.View.index_ref", Untyped(fn_impls::SimpleCoroViewIndex));
  Register(map, "std.view.View.index_mut", Untyped(fn_impls::SimpleCoroViewIndex));
  Register(map, "std.view.View.slice_ref", Untyped(fn_impls::SimpleCoroViewSlice));
  Register(map, "std.view.View.slice_mut", Untyped(fn_impls::SimpleCoroViewSlice));
  Register(map, "std.view.View.iter_ref", CoroViewIter(false, true));
  Register(map, "std.view.View.iter_mut", CoroViewIter(false, true));
  Register(map, "std.view.View.reverse_iter_ref", CoroViewIter(true, true));
  Register(map, "std.view.View.reverse_iter_mut", CoroViewIter(true, true));

  Register(map, "std.mem.pointer.NonNull.read", fn_impls::StdNonNullRead);
  Register(map, "std.mem.pointer.NonNull.write", fn_impls::StdNonNullWrite);
  Register(map, "std.mem.pointer.NonNull.raw", fn_impls::StdNonNullRaw);
  Register(map, "std.mem.pointer.NonNull.erase_type", fn_impls::StdNonNullEraseType);
  Register(map, "std.function.FunMov.drop", fn_impls::StdFunctionFunMovDrop);
  Register(map, "std.cffi.CClosure.from", fn_impls::StdCffiCClosureFrom);

  Register(map, "std.mem.pointer.NonNull.cast", fn_impls::StdNonNullCast);
  Register(map, "std.mem.pointer.NonNull.from_ptr_inner", fn_impls::StdNonNullFromPtrInner);
  Register(map, "std.mem.pointer.NonNull.dangling", fn_impls::StdNonNullDangling);
  Register(map, "std.mem.pointer.Ptr.null", fn_impls::StdPtrNull);
  Register(map, "std.mem.pointer.Ptr.is_null", fn_impls::StdPtrIsNull);
  Register(map, "std.mem.pointer.Ptr.addr", fn_impls::StdPtrAddr);
  Register(map, "std.mem.pointer.NonNull.from_mut", fn_impls::StdNonNullFromMut);
  Register(map, "std.mem.pointer.NonNull.fwd_mut", Untyped(fn_impls::SimpleCoroNonNullFwd));
  Register(map, "std.mem.pointer.NonNull.fwd_ref", Untyped(fn_impls::SimpleCoroNonNullFwd));

  Register(map, "std.mem.volatile.Vol.read", fn_impls::StdVolRead);
  Register(map, "std.mem.volatile.Vol.write", fn_impls::StdVolWrite);
  Register(map, "std.mem.volatile.Vol.swap", fn_impls::StdVolReplace);

  Register(map, "std.mem.raw_buf.RawBuf.index_ref", Untyped(fn_impls::SimpleCoroViewIndex));
  Register(map, "std.mem.raw_buf.RawBuf.index_mut", Untyped(fn_impls::SimpleCoroViewIndex));
  Register(map, "std.mem.raw_buf.RawBuf.take_at", fn_impls::StdRawBufTakeAt);
  Register(map, "std.mem.raw_buf.RawBuf.place_at", fn_impls::StdRawBufPlaceAt);
  Register(map, "std.mem.raw_buf.RawBuf.shift", fn_impls::StdRawBufShift);
  Register(map, "std.mem.raw_buf.RawBuf.clear_range", fn_impls::StdRawBufClearRange, nullptr, "T");

  Register(map, "std.mem.ops.size_of", fn_impls::StdMemOpsSizeOf, ct::MakeCompTimeFn<true>(ct::StdMemOpsSizeOf));
  Register(map, "std.mem.ops.size_of_val", fn_impls::StdMemOpsSizeOfVal);
  Register(map, "std.mem.ops.align_of", fn_impls::StdMemOpsAlignOf, ct::MakeCompTimeFn<true>(ct::StdMemOpsAlignOf));
  Register(map, "std.mem.ops.align_of_val", fn_impls::StdMemOpsAlignOfVal);
  Register(map, "std.mem.ops.replace", fn_impls::StdMemOpsReplace);
  Register(map, "std.mem.ops.swap", fn_impls::StdMemOpsSwap);
  Register(map, "std.mem.ops.transmute", fn_impls::StdMemOpsTransmute);
  Register(map, "std.mem.ops.mem_copy", fn_impls::StdMemOpsMemCopy);
  Register(map, "std.mem.ops.mem_set", fn_impls::StdMemOpsMemSet);
  Register(map, "std.mem.ops.drop", fn_impls::StdMemOpsDrop, nullptr, "T");

  Register(map, "std.hints.black_box", fn_impls::StdHintsBlackBox);
  Register(map, "std.hints.black_box_ref", fn_impls::StdHintsBlackBox);
  Register(
    map, "std.hints.likely", fn_impls::StdHintsBranch, ct::MakeCompTimeFn(ct::StdHintsBranch), "", Expect(true));
  Register(
    map, "std.hints.unlikely", fn_impls::StdHintsBranch, ct::MakeCompTimeFn(ct::StdHintsBranch), "", Expect(false));
  Register(
    map, "std.hints.cold_path", fn_impls::StdHintsColdPath, ct::MakeCompTimeFn(ct::StdHintsColdPath), "",
    fn_impls::StdHintsColdPathMarker);

  Register(
    map, "std.intrinsics.sadd", Binop(BinOp::SAddChecked), ct::MakeCompTimeFn(ct::StdIntrinsicsAdd));
  Register(
    map, "std.intrinsics.sadd_assign", BinopAssign(BinOp::SAddChecked), ct::MakeCompTimeFn(ct::StdIntrinsicsAddAssign));
  Register(
    map, "std.intrinsics.uadd", Binop(BinOp::UAddChecked), ct::MakeCompTimeFn(ct::StdIntrinsicsAdd));
  Register(
    map, "std.intrinsics.uadd_assign", BinopAssign(BinOp::UAddChecked), ct::MakeCompTimeFn(ct::StdIntrinsicsAddAssign));
  Register(
    map, "std.intrinsics.ssub", Binop(BinOp::SSubChecked), ct::MakeCompTimeFn(ct::StdIntrinsicsSub));
  Register(
    map, "std.intrinsics.ssub_assign", BinopAssign(BinOp::SSubChecked), ct::MakeCompTimeFn(ct::StdIntrinsicsSubAssign));
  Register(
    map, "std.intrinsics.usub", Binop(BinOp::USubChecked), ct::MakeCompTimeFn(ct::StdIntrinsicsSub));
  Register(
    map, "std.intrinsics.usub_assign", BinopAssign(BinOp::USubChecked), ct::MakeCompTimeFn(ct::StdIntrinsicsSubAssign));
  Register(
    map, "std.intrinsics.smul", Binop(BinOp::SMulChecked), ct::MakeCompTimeFn(ct::StdIntrinsicsMul));
  Register(
    map, "std.intrinsics.smul_assign", BinopAssign(BinOp::SMulChecked), ct::MakeCompTimeFn(ct::StdIntrinsicsMulAssign));
  Register(
    map, "std.intrinsics.umul", Binop(BinOp::UMulChecked), ct::MakeCompTimeFn(ct::StdIntrinsicsMul));
  Register(
    map, "std.intrinsics.umul_assign", BinopAssign(BinOp::UMulChecked), ct::MakeCompTimeFn(ct::StdIntrinsicsMulAssign));
  Register(
    map, "std.intrinsics.sdiv", Binop(BinOp::SDiv), ct::MakeCompTimeFn(ct::StdIntrinsicsDiv));
  Register(
    map, "std.intrinsics.sdiv_assign", BinopAssign(BinOp::SDiv), ct::MakeCompTimeFn(ct::StdIntrinsicsDivAssign));
  Register(
    map, "std.intrinsics.udiv", Binop(BinOp::UDiv), ct::MakeCompTimeFn(ct::StdIntrinsicsDiv));
  Register(
    map, "std.intrinsics.udiv_assign", BinopAssign(BinOp::UDiv), ct::MakeCompTimeFn(ct::StdIntrinsicsDivAssign));
  Register(
    map, "std.intrinsics.srem", Binop(BinOp::SRem), ct::MakeCompTimeFn(ct::StdIntrinsicsRem));
  Register(
    map, "std.intrinsics.srem_assign", BinopAssign(BinOp::SRem), ct::MakeCompTimeFn(ct::StdIntrinsicsRemAssign));
  Register(
    map, "std.intrinsics.urem", Binop(BinOp::URem), ct::MakeCompTimeFn(ct::StdIntrinsicsRem));
  Register(
    map, "std.intrinsics.urem_assign", BinopAssign(BinOp::URem), ct::MakeCompTimeFn(ct::StdIntrinsicsRemAssign));
  Register(
    map, "std.intrinsics.sneg", Unop(UnOp::Neg), ct::MakeCompTimeFn(ct::StdIntrinsicsSneg));
  Register(
    map, "std.intrinsics.bit_shl", Binop(BinOp::Shl), ct::MakeCompTimeFn(ct::StdIntrinsicsBitShl));
  Register(
    map, "std.intrinsics.bit_shl_assign", BinopAssign(BinOp::Shl), ct::MakeCompTimeFn(ct::StdIntrinsicsBitShlAssign));
  Register(
    map, "std.intrinsics.bit_shr", Binop(BinOp::LShr), ct::MakeCompTimeFn(ct::StdIntrinsicsBitShr));
  Register(
    map, "std.intrinsics.bit_shr_assign", BinopAssign(BinOp::LShr), ct::MakeCompTimeFn(ct::StdIntrinsicsBitShrAssign));
  Register(
    map, "std.intrinsics.bit_ashr", Binop(BinOp::AShr), ct::MakeCompTimeFn(ct::StdIntrinsicsBitAShr));
  Register(
    map, "std.intrinsics.bit_ashr_assign", BinopAssign(BinOp::AShr), ct::MakeCompTimeFn(ct::StdIntrinsicsBitAShrAssign));
  Register(
    map, "std.intrinsics.bit_ior", Binop(BinOp::Or), ct::MakeCompTimeFn(ct::StdIntrinsicsBitIor));
  Register(
    map, "std.intrinsics.bit_ior_assign", BinopAssign(BinOp::Or), ct::MakeCompTimeFn(ct::StdIntrinsicsBitIorAssign));
  Register(
    map, "std.intrinsics.bit_and", Binop(BinOp::And), ct::MakeCompTimeFn(ct::StdIntrinsicsBitAnd));
  Register(
    map, "std.intrinsics.bit_and_assign", BinopAssign(BinOp::And), ct::MakeCompTimeFn(ct::StdIntrinsicsBitAndAssign));
  Register(
    map, "std.intrinsics.bit_xor", Binop(BinOp::Xor), ct::MakeCompTimeFn(ct::StdIntrinsicsBitXor));
  Register(
    map, "std.intrinsics.bit_xor_assign", BinopAssign(BinOp::Xor), ct::MakeCompTimeFn(ct::StdIntrinsicsBitXorAssign));
  Register(
    map, "std.intrinsics.bit_not", Unop(UnOp::Not), ct::MakeCompTimeFn(ct::StdIntrinsicsBitNot));
  Register(
    map, "std.intrinsics.bit_not_assign", UnopAssign(UnOp::Not), ct::MakeCompTimeFn(ct::StdIntrinsicsBitNotAssign));
  Register(
    map, "std.intrinsics.eq", Binop(BinOp::ICmpEQ), ct::MakeCompTimeFn(ct::StdIntrinsicsEq));
  Register(
    map, "std.intrinsics.oeq", Binop(BinOp::FCmpOEQ), ct::MakeCompTimeFn(ct::StdIntrinsicsOeq));
  Register(
    map, "std.intrinsics.ne", Binop(BinOp::ICmpNE), ct::MakeCompTimeFn(ct::StdIntrinsicsNe));
  Register(
    map, "std.intrinsics.une", Binop(BinOp::FCmpUNE), ct::MakeCompTimeFn(ct::StdIntrinsicsUne));
  Register(
    map, "std.intrinsics.slt", Binop(BinOp::ICmpSLT), ct::MakeCompTimeFn(ct::StdIntrinsicsLt));
  Register(
    map, "std.intrinsics.ult", Binop(BinOp::ICmpULT), ct::MakeCompTimeFn(ct::StdIntrinsicsLt));
  Register(
    map, "std.intrinsics.olt", Binop(BinOp::FCmpOLT), ct::MakeCompTimeFn(ct::StdIntrinsicsOlt));
  Register(
    map, "std.intrinsics.sle", Binop(BinOp::ICmpSLE), ct::MakeCompTimeFn(ct::StdIntrinsicsLe));
  Register(
    map, "std.intrinsics.ule", Binop(BinOp::ICmpULE), ct::MakeCompTimeFn(ct::StdIntrinsicsLe));
  Register(
    map, "std.intrinsics.ole", Binop(BinOp::FCmpOLE), ct::MakeCompTimeFn(ct::StdIntrinsicsOle));
  Register(
    map, "std.intrinsics.sgt", Binop(BinOp::ICmpSGT), ct::MakeCompTimeFn(ct::StdIntrinsicsGt));
  Register(
    map, "std.intrinsics.ugt", Binop(BinOp::ICmpUGT), ct::MakeCompTimeFn(ct::StdIntrinsicsGt));
  Register(
    map, "std.intrinsics.ogt", Binop(BinOp::FCmpOGT), ct::MakeCompTimeFn(ct::StdIntrinsicsOgt));
  Register(
    map, "std.intrinsics.sge", Binop(BinOp::ICmpSGE), ct::MakeCompTimeFn(ct::StdIntrinsicsGe));
  Register(
    map, "std.intrinsics.uge", Binop(BinOp::ICmpUGE), ct::MakeCompTimeFn(ct::StdIntrinsicsGe));
  Register(
    map, "std.intrinsics.oge", Binop(BinOp::FCmpOGE), ct::MakeCompTimeFn(ct::StdIntrinsicsOge));
  Register(
    map, "std.intrinsics.min_val", fn_impls::StdIntrinsicsMinVal, ct::MakeCompTimeFn<true>(ct::StdIntrinsicsMinVal));
  Register(
    map, "std.intrinsics.max_val", fn_impls::StdIntrinsicsMaxVal, ct::MakeCompTimeFn<true>(ct::StdIntrinsicsMaxVal));
  Register(
    map, "std.intrinsics.smax", BinaryIntrinsic(llvm::Intrinsic::smax), ct::MakeCompTimeFn(ct::StdIntrinsicsMax));
  Register(
    map, "std.intrinsics.umax", BinaryIntrinsic(llvm::Intrinsic::umax), ct::MakeCompTimeFn(ct::StdIntrinsicsMax));
  Register(
    map, "std.intrinsics.smin", BinaryIntrinsic(llvm::Intrinsic::smin), ct::MakeCompTimeFn(ct::StdIntrinsicsMin));
  Register(
    map, "std.intrinsics.umin", BinaryIntrinsic(llvm::Intrinsic::umin), ct::MakeCompTimeFn(ct::StdIntrinsicsMin));
  Register(
    map, "std.intrinsics.scmp", fn_impls::StdIntrinsicsScmp, ct::MakeCompTimeFn(ct::StdIntrinsicsCmp));
  Register(
    map, "std.intrinsics.ucmp", fn_impls::StdIntrinsicsUcmp, ct::MakeCompTimeFn(ct::StdIntrinsicsCmp));
  // Float arithmetic is checked as integer arithmetic is (when "[binary.codegen] float_checks" is on): "float_add" and
  // the rest stop on IEEE's invalid, division-by-zero and overflow exceptions. "*_ieee" is IEEE's non-stop result,
  // as "*_wrapping" is for integers, and "*_raised" the result and whether it raised, as "*_overflow" is.
  Register(
    map, "std.intrinsics.float_add", Binop(BinOp::FAddChecked), ct::MakeCompTimeFn(ct::StdIntrinsicsFadd));
  Register(
    map, "std.intrinsics.float_add_assign", BinopAssign(BinOp::FAddChecked),
    ct::MakeCompTimeFn(ct::StdIntrinsicsFaddAssign));
  Register(
    map, "std.intrinsics.float_add_ieee", Binop(BinOp::FAdd), ct::MakeCompTimeFn(ct::StdIntrinsicsFadd));
  Register(map, "std.intrinsics.float_add_raised", FloatRaised(BinOp::FAdd, false));
  Register(
    map, "std.intrinsics.float_sub", Binop(BinOp::FSubChecked), ct::MakeCompTimeFn(ct::StdIntrinsicsFsub));
  Register(
    map, "std.intrinsics.float_sub_assign", BinopAssign(BinOp::FSubChecked),
    ct::MakeCompTimeFn(ct::StdIntrinsicsFsubAssign));
  Register(
    map, "std.intrinsics.float_sub_ieee", Binop(BinOp::FSub), ct::MakeCompTimeFn(ct::StdIntrinsicsFsub));
  Register(map, "std.intrinsics.float_sub_raised", FloatRaised(BinOp::FSub, false));
  Register(
    map, "std.intrinsics.float_mul", Binop(BinOp::FMulChecked), ct::MakeCompTimeFn(ct::StdIntrinsicsFmul));
  Register(
    map, "std.intrinsics.float_mul_assign", BinopAssign(BinOp::FMulChecked),
    ct::MakeCompTimeFn(ct::StdIntrinsicsFmulAssign));
  Register(
    map, "std.intrinsics.float_mul_ieee", Binop(BinOp::FMul), ct::MakeCompTimeFn(ct::StdIntrinsicsFmul));
  Register(map, "std.intrinsics.float_mul_raised", FloatRaised(BinOp::FMul, false));
  Register(
    map, "std.intrinsics.float_div", Binop(BinOp::FDivChecked), ct::MakeCompTimeFn(ct::StdIntrinsicsFdiv));
  Register(
    map, "std.intrinsics.float_div_assign", BinopAssign(BinOp::FDivChecked),
    ct::MakeCompTimeFn(ct::StdIntrinsicsFdivAssign));
  Register(
    map, "std.intrinsics.float_div_ieee", Binop(BinOp::FDiv), ct::MakeCompTimeFn(ct::StdIntrinsicsFdiv));
  Register(map, "std.intrinsics.float_div_raised", FloatRaised(BinOp::FDiv, false));
  Register(
    map, "std.intrinsics.float_rem", Binop(BinOp::FRemChecked), ct::MakeCompTimeFn(ct::StdIntrinsicsFrem));
  Register(
    map, "std.intrinsics.float_rem_assign", BinopAssign(BinOp::FRemChecked),
    ct::MakeCompTimeFn(ct::StdIntrinsicsFremAssign));
  Register(
    map, "std.intrinsics.float_rem_ieee", Binop(BinOp::FRem), ct::MakeCompTimeFn(ct::StdIntrinsicsFrem));
  Register(map, "std.intrinsics.float_rem_raised", FloatRaised(BinOp::FRem, false));
  Register(
    map, "std.intrinsics.float_neg", Unop(UnOp::FNeg), ct::MakeCompTimeFn(ct::StdIntrinsicsFneg));
  Register(map, "std.intrinsics.float_sqrt", Unop(UnOp::FSqrtChecked));
  Register(map, "std.intrinsics.float_sqrt_ieee", Unop(UnOp::FSqrt));
  Register(map, "std.intrinsics.float_sqrt_raised", FloatRaised(BinOp::FAdd, true));
  Register(map, "std.intrinsics.float_powf", FloatMath(FloatMathFn::Pow));
  Register(map, "std.intrinsics.float_powf_ieee", BinaryIntrinsic(llvm::Intrinsic::pow));
  Register(map, "std.intrinsics.float_sin", FloatMath(FloatMathFn::Sin));
  Register(map, "std.intrinsics.float_sin_ieee", UnaryIntrinsic(llvm::Intrinsic::sin));
  Register(map, "std.intrinsics.float_cos", FloatMath(FloatMathFn::Cos));
  Register(map, "std.intrinsics.float_cos_ieee", UnaryIntrinsic(llvm::Intrinsic::cos));
  Register(map, "std.intrinsics.float_tan", FloatMath(FloatMathFn::Tan));
  Register(map, "std.intrinsics.float_tan_ieee", UnaryIntrinsic(llvm::Intrinsic::tan));
  Register(map, "std.intrinsics.float_asin", FloatMath(FloatMathFn::Asin));
  Register(map, "std.intrinsics.float_asin_ieee", UnaryIntrinsic(llvm::Intrinsic::asin));
  Register(map, "std.intrinsics.float_acos", FloatMath(FloatMathFn::Acos));
  Register(map, "std.intrinsics.float_acos_ieee", UnaryIntrinsic(llvm::Intrinsic::acos));
  Register(map, "std.intrinsics.float_atan", FloatMath(FloatMathFn::Atan));
  Register(map, "std.intrinsics.float_atan_ieee", UnaryIntrinsic(llvm::Intrinsic::atan));
  Register(map, "std.intrinsics.float_atan2", FloatMath(FloatMathFn::Atan2));
  Register(map, "std.intrinsics.float_atan2_ieee", BinaryIntrinsic(llvm::Intrinsic::atan2));
  Register(map, "std.intrinsics.float_sinh", FloatMath(FloatMathFn::Sinh));
  Register(map, "std.intrinsics.float_sinh_ieee", UnaryIntrinsic(llvm::Intrinsic::sinh));
  Register(map, "std.intrinsics.float_cosh", FloatMath(FloatMathFn::Cosh));
  Register(map, "std.intrinsics.float_cosh_ieee", UnaryIntrinsic(llvm::Intrinsic::cosh));
  Register(map, "std.intrinsics.float_tanh", FloatMath(FloatMathFn::Tanh));
  Register(map, "std.intrinsics.float_tanh_ieee", UnaryIntrinsic(llvm::Intrinsic::tanh));
  Register(map, "std.intrinsics.float_exp", FloatMath(FloatMathFn::Exp));
  Register(map, "std.intrinsics.float_exp_ieee", UnaryIntrinsic(llvm::Intrinsic::exp));
  Register(map, "std.intrinsics.float_exp2", FloatMath(FloatMathFn::Exp2));
  Register(map, "std.intrinsics.float_exp2_ieee", UnaryIntrinsic(llvm::Intrinsic::exp2));
  Register(map, "std.intrinsics.float_exp10", FloatMath(FloatMathFn::Exp10));
  Register(map, "std.intrinsics.float_exp10_ieee", UnaryIntrinsic(llvm::Intrinsic::exp10));
  Register(map, "std.intrinsics.float_log", FloatMath(FloatMathFn::Log));
  Register(map, "std.intrinsics.float_log_ieee", UnaryIntrinsic(llvm::Intrinsic::log));
  Register(map, "std.intrinsics.float_log2", FloatMath(FloatMathFn::Log2));
  Register(map, "std.intrinsics.float_log2_ieee", UnaryIntrinsic(llvm::Intrinsic::log2));
  Register(map, "std.intrinsics.float_log10", FloatMath(FloatMathFn::Log10));
  Register(map, "std.intrinsics.float_log10_ieee", UnaryIntrinsic(llvm::Intrinsic::log10));
  Register(
    map, "std.intrinsics.float_abs", UnaryIntrinsic(llvm::Intrinsic::fabs), ct::MakeCompTimeFn(ct::StdIntrinsicsFabs));
  Register(
    map, "std.intrinsics.float_max_val", fn_impls::StdIntrinsicsFmaxVal,
    ct::MakeCompTimeFn<true>(ct::StdIntrinsicsFmaxVal));
  Register(
    map, "std.intrinsics.float_min_val", fn_impls::StdIntrinsicsFminVal,
    ct::MakeCompTimeFn<true>(ct::StdIntrinsicsFminVal));
  Register(
    map, "std.intrinsics.float_max", BinaryIntrinsic(llvm::Intrinsic::maximum),
    ct::MakeCompTimeFn(ct::StdIntrinsicsFmax));
  Register(
    map, "std.intrinsics.float_min", BinaryIntrinsic(llvm::Intrinsic::minimum),
    ct::MakeCompTimeFn(ct::StdIntrinsicsFmin));
  Register(
    map, "std.intrinsics.float_copysign", BinaryIntrinsic(llvm::Intrinsic::copysign));
  Register(
    map, "std.intrinsics.float_floor", UnaryIntrinsic(llvm::Intrinsic::floor),
    ct::MakeCompTimeFn(ct::StdIntrinsicsFfloor));
  Register(
    map, "std.intrinsics.float_ceil", UnaryIntrinsic(llvm::Intrinsic::ceil),
    ct::MakeCompTimeFn(ct::StdIntrinsicsFceil));
  Register(
    map, "std.intrinsics.float_trunc", UnaryIntrinsic(llvm::Intrinsic::trunc),
    ct::MakeCompTimeFn(ct::StdIntrinsicsFtrunc));
  Register(
    map, "std.intrinsics.float_round", UnaryIntrinsic(llvm::Intrinsic::round),
    ct::MakeCompTimeFn(ct::StdIntrinsicsFround));
  Register(
    map, "std.intrinsics.float_round_even", UnaryIntrinsic(llvm::Intrinsic::roundeven),
    ct::MakeCompTimeFn(ct::StdIntrinsicsFroundEven));
  Register(
    map, "std.intrinsics.bitreverse", UnaryIntrinsic(llvm::Intrinsic::bitreverse));
  Register(map, "std.intrinsics.bswap", fn_impls::StdIntrinsicsBswap);
  Register(map, "std.intrinsics.ctlz", BitCount(llvm::Intrinsic::ctlz));
  Register(map, "std.intrinsics.cttz", BitCount(llvm::Intrinsic::cttz));
  Register(map, "std.intrinsics.ctpop", BitCount(llvm::Intrinsic::ctpop));
  Register(map, "std.intrinsics.fshl", FunnelShift(llvm::Intrinsic::fshl));
  Register(map, "std.intrinsics.fshr", FunnelShift(llvm::Intrinsic::fshr));

  Register(map, "std.intrinsics.sadd_overflow", OverflowIntrinsic(llvm::Intrinsic::sadd_with_overflow));
  Register(map, "std.intrinsics.uadd_overflow", OverflowIntrinsic(llvm::Intrinsic::uadd_with_overflow));
  Register(map, "std.intrinsics.ssub_overflow", OverflowIntrinsic(llvm::Intrinsic::ssub_with_overflow));
  Register(map, "std.intrinsics.usub_overflow", OverflowIntrinsic(llvm::Intrinsic::usub_with_overflow));
  Register(map, "std.intrinsics.smul_overflow", OverflowIntrinsic(llvm::Intrinsic::smul_with_overflow));
  Register(map, "std.intrinsics.umul_overflow", OverflowIntrinsic(llvm::Intrinsic::umul_with_overflow));
  Register(map, "std.intrinsics.sadd_saturating", BinaryIntrinsic(llvm::Intrinsic::sadd_sat));
  Register(map, "std.intrinsics.uadd_saturating", BinaryIntrinsic(llvm::Intrinsic::uadd_sat));
  Register(map, "std.intrinsics.ssub_saturating", BinaryIntrinsic(llvm::Intrinsic::ssub_sat));
  Register(map, "std.intrinsics.usub_saturating", BinaryIntrinsic(llvm::Intrinsic::usub_sat));
  Register(map, "std.intrinsics.sshl_saturating", fn_impls::StdIntrinsicsSshlSaturating);
  Register(map, "std.intrinsics.ushl_saturating", fn_impls::StdIntrinsicsUshlSaturating);
  Register(map, "std.intrinsics.sadd_wrapping", Binop(BinOp::Add));
  Register(map, "std.intrinsics.uadd_wrapping", Binop(BinOp::Add));
  Register(map, "std.intrinsics.ssub_wrapping", Binop(BinOp::Sub));
  Register(map, "std.intrinsics.usub_wrapping", Binop(BinOp::Sub));
  Register(map, "std.intrinsics.smul_wrapping", Binop(BinOp::Mul));
  Register(map, "std.intrinsics.umul_wrapping", Binop(BinOp::Mul));

  Register(map, "std.intrinsics.sitofp", Conv(ConvOp::SIToFP));
  Register(map, "std.intrinsics.uitofp", Conv(ConvOp::UIToFP));
  Register(map, "std.intrinsics.fptrunc", Conv(ConvOp::FPTrunc));
  // A float narrowed with no check (IEEE's infinity past the range), as "float_*_ieee" are.
  Register(map, "std.intrinsics.fptrunc_ieee", Conv(ConvOp::FPTruncIeee));
  Register(map, "std.intrinsics.strunc", Conv(ConvOp::Trunc));
  Register(map, "std.intrinsics.utrunc", Conv(ConvOp::Trunc));
  Register(map, "std.intrinsics.szext", Conv(ConvOp::SExt));
  Register(map, "std.intrinsics.uzext", Conv(ConvOp::ZExt));
  Register(map, "std.intrinsics.fpext", Conv(ConvOp::FPExt));
  Register(map, "std.intrinsics.bit_cast", Conv(ConvOp::BitCast));

  Register(map, "std.intrinsics.fptosi", Conv(ConvOp::FPToSI));
  Register(map, "std.intrinsics.fptoui", Conv(ConvOp::FPToUI));
  Register(map, "std.intrinsics.fpclass", fn_impls::StdIntrinsicsFpclass);

  Register(
    map, "std.num.sized_floating_point.SizedFloatingPoint.neg_one", fn_impls::StdNumFloatNegOne,
    ct::MakeCompTimeFn<true>(ct::StdNumFloatNegOne));
  Register(
    map, "std.num.sized_floating_point.SizedFloatingPoint.is_zero", IsConst(true, 0.0));
  Register(
    map, "std.num.sized_floating_point.SizedFloatingPoint.zero", fn_impls::StdNumFloatZero,
    ct::MakeCompTimeFn<true>(ct::StdNumFloatZero));
  Register(
    map, "std.num.sized_floating_point.SizedFloatingPoint.is_one", IsConst(true, 1.0));
  Register(
    map, "std.num.sized_floating_point.SizedFloatingPoint.one", fn_impls::StdNumFloatOne,
    ct::MakeCompTimeFn<true>(ct::StdNumFloatOne));
  Register(
    map, "std.num.sized_integer_signed.SizedIntegerSigned.neg_one", fn_impls::StdNumIntNegOne,
    ct::MakeCompTimeFn<true>(ct::StdNumIntNegOne));
  Register(
    map, "std.num.sized_integer.SizedInteger.is_zero", IsConst(false, 0.0));
  Register(
    map, "std.num.sized_integer.SizedInteger.zero", fn_impls::StdNumIntZero,
    ct::MakeCompTimeFn<true>(ct::StdNumIntZero));
  Register(
    map, "std.num.sized_integer.SizedInteger.one", fn_impls::StdNumIntOne, ct::MakeCompTimeFn<true>(ct::StdNumIntOne));
  Register(
    map, "std.num.sized_integer.SizedInteger.two", fn_impls::StdNumIntTwo, ct::MakeCompTimeFn<true>(ct::StdNumIntTwo));

  Register(map, "std.debug.breakpoint_internal", fn_impls::StdDebugBreakpointInternal);

  Register(map, "std.threading.atomic.atomic_fence_inner", fn_impls::StdThreadingAtomicFenceInner);
  Register(map, "std.threading.atomic.atomic_load_inner", fn_impls::StdThreadingAtomicLoadInner);
  Register(map, "std.threading.atomic.atomic_store_inner", fn_impls::StdThreadingAtomicStoreInner);
  Register(map, "std.threading.atomic.atomic_compex_inner", fn_impls::StdThreadingAtomicCompexInner);
  Register(map, "std.threading.atomic.atomic_compex_weak_inner", fn_impls::StdThreadingAtomicCompexWeakInner);
  Register(map, "std.threading.atomic.Atom.exchange", AtomicRmw(AtomicRmwOp::Xchg));
  Register(map, "std.threading.atomic.Atom.fetch_and", AtomicRmw(AtomicRmwOp::And));
  Register(map, "std.threading.atomic.Atom.fetch_nand", AtomicRmw(AtomicRmwOp::Nand));
  Register(map, "std.threading.atomic.Atom.fetch_or", AtomicRmw(AtomicRmwOp::Or));
  Register(map, "std.threading.atomic.Atom.fetch_xor", AtomicRmw(AtomicRmwOp::Xor));
  Register(map, "std.threading.atomic.Atom.is_lock_free", fn_impls::StdThreadingAtomicIsLockFree);
  Register(map, "std.threading.atomic.Atom.fetch_not", fn_impls::StdThreadingAtomicFetchNot);
  Register(map, "std.threading.atomic.Atom.fetch_add", AtomicRmw(AtomicRmwOp::Add));
  Register(map, "std.threading.atomic.Atom.fetch_sub", AtomicRmw(AtomicRmwOp::Sub));
  Register(map, "std.threading.atomic.Atom.fetch_fadd", AtomicRmw(AtomicRmwOp::FAdd));
  Register(map, "std.threading.atomic.Atom.fetch_fsub", AtomicRmw(AtomicRmwOp::FSub));
  Register(map, "std.threading.atomic.Atom.fetch_fmax", AtomicRmw(AtomicRmwOp::FMaximum));
  Register(map, "std.threading.atomic.Atom.fetch_fmin", AtomicRmw(AtomicRmwOp::FMinimum));
  Register(map, "std.threading.atomic.Atom.fetch_smax", AtomicRmw(AtomicRmwOp::Max));
  Register(map, "std.threading.atomic.Atom.fetch_umax", AtomicRmw(AtomicRmwOp::UMax));
  Register(map, "std.threading.atomic.Atom.fetch_smin", AtomicRmw(AtomicRmwOp::Min));
  Register(map, "std.threading.atomic.Atom.fetch_umin", AtomicRmw(AtomicRmwOp::UMin));
  return map;
}

// Defined here, not inline in the interface: MSVC emits no definition
// for an exported inline variable with a dynamic initialiser.
const spp::Map<spp::Str, spp::codegen::builtins::LoweredFnImpl> spp::codegen::builtins::kBuiltinFuncs =
  MakeBuiltinFnMap();
