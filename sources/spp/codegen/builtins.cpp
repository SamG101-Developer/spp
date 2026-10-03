module spp.codegen.builtins;
import spp.analyse.utils.comp_time_intrinsics;
import spp.asts.boolean_literal_ast;
import spp.asts.float_literal_ast;
import spp.asts.integer_literal_ast;
import spp.codegen.llvm_fn_impls;

#define SPP_DEFINE_BUILTIN_FUNC(scoped_name, func_name) \
  map.emplace(scoped_name, LoweredFnImpl{             \
    .LlvmImpl=codegen::fn_impls::func_name,           \
    .CompTimeImpl=nullptr,                              \
    .Name=scoped_name,                                  \
    .DropsGn=""})

#define SPP_DEFINE_BUILTIN_FUNC_CMP(scoped_name, func_name, ...)                                                                             \
  map.emplace(scoped_name, LoweredFnImpl{                                                                                                  \
    .LlvmImpl=codegen::fn_impls::func_name,                                                                                                \
    .CompTimeImpl=analyse::utils::comp_time_intrinsics::MakeCompTimeFn __VA_OPT__(<__VA_ARGS__>) (analyse::utils::comp_time_intrinsics::func_name), \
    .Name=scoped_name,                                                                                                                       \
    .DropsGn=""})

// As "SPP_DEFINE_BUILTIN_FUNC_CMP", for a builtin whose
// codegen and comptime implementations are not named the same.
#define SPP_DEFINE_BUILTIN_FUNC_CMP2(scoped_name, llvm_name, cmp_name, ...)                                                                 \
  map.emplace(scoped_name, LoweredFnImpl{                                                                                                 \
    .LlvmImpl=codegen::fn_impls::llvm_name,                                                                                               \
    .CompTimeImpl=analyse::utils::comp_time_intrinsics::MakeCompTimeFn __VA_OPT__(<__VA_ARGS__>) (analyse::utils::comp_time_intrinsics::cmp_name), \
    .Name=scoped_name,                                                                                                                      \
    .DropsGn=""})

// As "SPP_DEFINE_BUILTIN_FUNC", for a builtin whose lowering
// destroys a value of one of its generic parameters.
#define SPP_DEFINE_BUILTIN_FUNC_DROPS(scoped_name, func_name, generic) \
  map.emplace(scoped_name, LoweredFnImpl{                            \
    .LlvmImpl=codegen::fn_impls::func_name,                          \
    .CompTimeImpl=nullptr,                                             \
    .Name=scoped_name,                                                 \
    .DropsGn=generic})

auto spp::codegen::builtins::MakeBuiltinFnMap()
  -> Map<Str, LoweredFnImpl> {
  auto map = Map<Str, LoweredFnImpl>{};

  SPP_DEFINE_BUILTIN_FUNC("std.array.Arr.new", StdArrayNew);
  SPP_DEFINE_BUILTIN_FUNC("std.array.Arr.iter_mov", StdArrayIterMov);
  SPP_DEFINE_BUILTIN_FUNC("std.array.Arr.reverse_iter_mov", StdArrayReverseIterMov);
  SPP_DEFINE_BUILTIN_FUNC("std.array.Arr.fwd_ref", StdArrayFwdRef);
  SPP_DEFINE_BUILTIN_FUNC("std.array.Arr.fwd_mut", StdArrayFwdMut);

  SPP_DEFINE_BUILTIN_FUNC("std.vector.Vec.fwd_ref", StdVectorFwdRef);
  SPP_DEFINE_BUILTIN_FUNC("std.vector.Vec.fwd_mut", StdVectorFwdMut);

  SPP_DEFINE_BUILTIN_FUNC("std.generator.Gen.send", StdGeneratorSend);
  SPP_DEFINE_BUILTIN_FUNC("std.generator.Gen.drop", StdGeneratorDrop);
  SPP_DEFINE_BUILTIN_FUNC("std.generator.GenOnce.drop", StdGeneratorDrop);
  SPP_DEFINE_BUILTIN_FUNC("std.generator.GenOnce.send", StdGeneratorOnceSend);

  SPP_DEFINE_BUILTIN_FUNC("std.string_view.StrView.slice_ref", StdStringViewSliceRef);
  SPP_DEFINE_BUILTIN_FUNC("std.string_view.StrView.slice_mut", StdStringViewSliceMut);

  SPP_DEFINE_BUILTIN_FUNC("std.view.View.index_ref", StdViewIndexRef);
  SPP_DEFINE_BUILTIN_FUNC("std.view.View.index_mut", StdViewIndexMut);
  SPP_DEFINE_BUILTIN_FUNC("std.view.View.slice_ref", StdViewSliceRef);
  SPP_DEFINE_BUILTIN_FUNC("std.view.View.slice_mut", StdViewSliceMut);
  SPP_DEFINE_BUILTIN_FUNC("std.view.View.iter_ref", StdViewIterRef);
  SPP_DEFINE_BUILTIN_FUNC("std.view.View.iter_mut", StdViewIterMut);
  SPP_DEFINE_BUILTIN_FUNC("std.view.View.reverse_iter_ref", StdViewReverseIterRef);
  SPP_DEFINE_BUILTIN_FUNC("std.view.View.reverse_iter_mut", StdViewReverseIterMut);

  SPP_DEFINE_BUILTIN_FUNC("std.mem.pointer.NonNull.read", StdNonNullRead);
  SPP_DEFINE_BUILTIN_FUNC("std.mem.pointer.NonNull.write", StdNonNullWrite);
  SPP_DEFINE_BUILTIN_FUNC("std.mem.pointer.NonNull.raw", StdNonNullRaw);
  SPP_DEFINE_BUILTIN_FUNC("std.mem.pointer.NonNull.erase_type", StdNonNullEraseType);
  SPP_DEFINE_BUILTIN_FUNC("std.function.FunMov.drop", StdFunctionFunMovDrop);
  SPP_DEFINE_BUILTIN_FUNC("std.cffi.CClosure.from", StdCffiCClosureFrom);

  SPP_DEFINE_BUILTIN_FUNC("std.mem.pointer.NonNull.cast", StdNonNullCast);
  SPP_DEFINE_BUILTIN_FUNC("std.mem.pointer.NonNull.from_ptr_inner", StdNonNullFromPtrInner);
  SPP_DEFINE_BUILTIN_FUNC("std.mem.pointer.NonNull.fwd_mut", StdNonNullFwdMut);
  SPP_DEFINE_BUILTIN_FUNC("std.mem.pointer.NonNull.fwd_ref", StdNonNullFwdRef);

  SPP_DEFINE_BUILTIN_FUNC("std.mem.volatile.Vol.read", StdVolRead);
  SPP_DEFINE_BUILTIN_FUNC("std.mem.volatile.Vol.write", StdVolWrite);
  SPP_DEFINE_BUILTIN_FUNC("std.mem.volatile.Vol.swap", StdVolReplace);

  SPP_DEFINE_BUILTIN_FUNC("std.mem.raw_buf.RawBuf.index_ref", StdRawBufIndexRef);
  SPP_DEFINE_BUILTIN_FUNC("std.mem.raw_buf.RawBuf.index_mut", StdRawBufIndexMut);
  SPP_DEFINE_BUILTIN_FUNC("std.mem.raw_buf.RawBuf.take_at", StdRawBufTakeAt);
  SPP_DEFINE_BUILTIN_FUNC("std.mem.raw_buf.RawBuf.place_at", StdRawBufPlaceAt);
  SPP_DEFINE_BUILTIN_FUNC("std.mem.raw_buf.RawBuf.shift", StdRawBufShift);
  SPP_DEFINE_BUILTIN_FUNC("std.mem.raw_buf.RawBuf.clear_range", StdRawBufClearRange);

  SPP_DEFINE_BUILTIN_FUNC_CMP("std.mem.ops.size_of", StdMemOpsSizeOf, true);
  SPP_DEFINE_BUILTIN_FUNC("std.mem.ops.size_of_val", StdMemOpsSizeOfVal);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.mem.ops.align_of", StdMemOpsAlignOf, true);
  SPP_DEFINE_BUILTIN_FUNC("std.mem.ops.align_of_val", StdMemOpsAlignOfVal);
  SPP_DEFINE_BUILTIN_FUNC("std.mem.ops.replace", StdMemOpsReplace);
  SPP_DEFINE_BUILTIN_FUNC_DROPS("std.mem.ops.drop", StdMemOpsDrop, "T");
  SPP_DEFINE_BUILTIN_FUNC_DROPS("std.mem.ops.drop_in_place", StdMemOpsDropInPlace, "T");

  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.sadd", StdIntrinsicsSadd, StdIntrinsicsAdd);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.sadd_assign", StdIntrinsicsSaddAssign, StdIntrinsicsAddAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.uadd", StdIntrinsicsUadd, StdIntrinsicsAdd);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.uadd_assign", StdIntrinsicsUaddAssign, StdIntrinsicsAddAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.ssub", StdIntrinsicsSsub, StdIntrinsicsSub);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.ssub_assign", StdIntrinsicsSsubAssign, StdIntrinsicsSubAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.usub", StdIntrinsicsUsub, StdIntrinsicsSub);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.usub_assign", StdIntrinsicsUsubAssign, StdIntrinsicsSubAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.smul", StdIntrinsicsSmul, StdIntrinsicsMul);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.smul_assign", StdIntrinsicsSmulAssign, StdIntrinsicsMulAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.umul", StdIntrinsicsUmul, StdIntrinsicsMul);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.umul_assign", StdIntrinsicsUmulAssign, StdIntrinsicsMulAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.sdiv", StdIntrinsicsSdiv, StdIntrinsicsDiv);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.sdiv_assign", StdIntrinsicsSdivAssign, StdIntrinsicsDivAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.udiv", StdIntrinsicsUdiv, StdIntrinsicsDiv);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.udiv_assign", StdIntrinsicsUdivAssign, StdIntrinsicsDivAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.srem", StdIntrinsicsSrem, StdIntrinsicsRem);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.srem_assign", StdIntrinsicsSremAssign, StdIntrinsicsRemAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.urem", StdIntrinsicsUrem, StdIntrinsicsRem);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.urem_assign", StdIntrinsicsUremAssign, StdIntrinsicsRemAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.sneg", StdIntrinsicsSneg);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.bit_shl", StdIntrinsicsBitShl);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.bit_shl_assign", StdIntrinsicsBitShlAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.bit_shr", StdIntrinsicsBitShr);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.bit_shr_assign", StdIntrinsicsBitShrAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.bit_ior", StdIntrinsicsBitIor);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.bit_ior_assign", StdIntrinsicsBitIorAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.bit_and", StdIntrinsicsBitAnd);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.bit_and_assign", StdIntrinsicsBitAndAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.bit_xor", StdIntrinsicsBitXor);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.bit_xor_assign", StdIntrinsicsBitXorAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.bit_not", StdIntrinsicsBitNot);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.bit_not_assign", StdIntrinsicsBitNotAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.abs", StdIntrinsicsAbs);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.eq", StdIntrinsicsEq);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.oeq", StdIntrinsicsOeq);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.ne", StdIntrinsicsNe);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.one", StdIntrinsicsOne);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.slt", StdIntrinsicsSlt, StdIntrinsicsLt);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.ult", StdIntrinsicsUlt, StdIntrinsicsLt);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.olt", StdIntrinsicsOlt);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.sle", StdIntrinsicsSle, StdIntrinsicsLe);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.ule", StdIntrinsicsUle, StdIntrinsicsLe);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.ole", StdIntrinsicsOle);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.sgt", StdIntrinsicsSgt, StdIntrinsicsGt);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.ugt", StdIntrinsicsUgt, StdIntrinsicsGt);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.ogt", StdIntrinsicsOgt);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.sge", StdIntrinsicsSge, StdIntrinsicsGe);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.uge", StdIntrinsicsUge, StdIntrinsicsGe);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.oge", StdIntrinsicsOge);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.min_val", StdIntrinsicsMinVal, true);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.max_val", StdIntrinsicsMaxVal, true);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.smax", StdIntrinsicsSmax, StdIntrinsicsMax);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.umax", StdIntrinsicsUmax, StdIntrinsicsMax);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.smin", StdIntrinsicsSmin, StdIntrinsicsMin);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.umin", StdIntrinsicsUmin, StdIntrinsicsMin);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.scmp", StdIntrinsicsScmp, StdIntrinsicsCmp);
  SPP_DEFINE_BUILTIN_FUNC_CMP2("std.intrinsics.ucmp", StdIntrinsicsUcmp, StdIntrinsicsCmp);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_add", StdIntrinsicsFadd);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_add_assign", StdIntrinsicsFaddAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_sub", StdIntrinsicsFsub);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_sub_assign", StdIntrinsicsFsubAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_mul", StdIntrinsicsFmul);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_mul_assign", StdIntrinsicsFmulAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_div", StdIntrinsicsFdiv);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_div_assign", StdIntrinsicsFdivAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_rem", StdIntrinsicsFrem);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_rem_assign", StdIntrinsicsFremAssign);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_neg", StdIntrinsicsFneg);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_sqrt", StdIntrinsicsFsqrt);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_powi", StdIntrinsicsFpowi);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_powf", StdIntrinsicsFpowf);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_sin", StdIntrinsicsFsin);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_cos", StdIntrinsicsFcos);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_tan", StdIntrinsicsFtan);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_asin", StdIntrinsicsFasin);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_acos", StdIntrinsicsFacos);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_atan", StdIntrinsicsFatan);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_atan2", StdIntrinsicsFatan2);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_sinh", StdIntrinsicsFsinh);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_cosh", StdIntrinsicsFcosh);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_tanh", StdIntrinsicsFtanh);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_exp", StdIntrinsicsFexp);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_exp2", StdIntrinsicsFexp2);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_exp10", StdIntrinsicsFexp10);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_log", StdIntrinsicsFlog);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_log2", StdIntrinsicsFlog2);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_log10", StdIntrinsicsFlog10);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_abs", StdIntrinsicsFabs);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_max_val", StdIntrinsicsFmaxVal, true);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_min_val", StdIntrinsicsFminVal, true);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_max", StdIntrinsicsFmax);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_min", StdIntrinsicsFmin);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.float_copysign", StdIntrinsicsFcopysign);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_floor", StdIntrinsicsFfloor);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_ceil", StdIntrinsicsFceil);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_trunc", StdIntrinsicsFtrunc);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.intrinsics.float_round", StdIntrinsicsFround);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.bitreverse", StdIntrinsicsBitreverse);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.ctlz", StdIntrinsicsCtlz);

  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.sadd_overflow", StdIntrinsicsSaddOverflow);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.uadd_overflow", StdIntrinsicsUaddOverflow);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.ssub_overflow", StdIntrinsicsSsubOverflow);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.usub_overflow", StdIntrinsicsUsubOverflow);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.smul_overflow", StdIntrinsicsSmulOverflow);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.umul_overflow", StdIntrinsicsUmulOverflow);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.sadd_saturating", StdIntrinsicsSaddSaturating);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.uadd_saturating", StdIntrinsicsUaddSaturating);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.ssub_saturating", StdIntrinsicsSsubSaturating);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.usub_saturating", StdIntrinsicsUsubSaturating);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.sshl_saturating", StdIntrinsicsSshlSaturating);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.ushl_saturating", StdIntrinsicsUshlSaturating);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.sadd_wrapping", StdIntrinsicsSaddWrapping);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.uadd_wrapping", StdIntrinsicsUaddWrapping);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.ssub_wrapping", StdIntrinsicsSsubWrapping);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.usub_wrapping", StdIntrinsicsUsubWrapping);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.smul_wrapping", StdIntrinsicsSmulWrapping);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.umul_wrapping", StdIntrinsicsUmulWrapping);

  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.sitofp", StdIntrinsicsSitofp);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.uitofp", StdIntrinsicsUitofp);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.fptrunc", StdIntrinsicsFptrunc);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.strunc", StdIntrinsicsStrunc);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.utrunc", StdIntrinsicsUtrunc);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.szext", StdIntrinsicsSzext);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.uzext", StdIntrinsicsUzext);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.fpext", StdIntrinsicsFpext);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.bit_cast", StdIntrinsicsBitCast);

  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.fptosi", StdIntrinsicsFptosi);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.fptoui", StdIntrinsicsFptoui);
  SPP_DEFINE_BUILTIN_FUNC("std.intrinsics.fpclass", StdIntrinsicsFpclass);

  SPP_DEFINE_BUILTIN_FUNC_CMP("std.num.sized_floating_point.SizedFloatingPoint.neg_one", StdNumFloatNegOne);
  SPP_DEFINE_BUILTIN_FUNC("std.num.sized_floating_point.SizedFloatingPoint.is_zero", StdNumFloatIsZero);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.num.sized_floating_point.SizedFloatingPoint.zero", StdNumFloatZero);
  SPP_DEFINE_BUILTIN_FUNC("std.num.sized_floating_point.SizedFloatingPoint.is_one", StdNumFloatIsOne);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.num.sized_floating_point.SizedFloatingPoint.one", StdNumFloatOne);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.num.sized_integer_signed.SizedIntegerSigned.neg_one", StdNumIntNegOne);
  SPP_DEFINE_BUILTIN_FUNC("std.num.sized_integer.SizedInteger.is_zero", StdNumIntIsZero);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.num.sized_integer.SizedInteger.zero", StdNumIntZero);
  SPP_DEFINE_BUILTIN_FUNC("std.num.sized_integer.SizedInteger.is_one", StdNumIntIsOne);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.num.sized_integer.SizedInteger.one", StdNumIntOne);
  SPP_DEFINE_BUILTIN_FUNC_CMP("std.num.sized_integer.SizedInteger.two", StdNumIntTwo);

  SPP_DEFINE_BUILTIN_FUNC("std.debug.breakpoint_internal", StdDebugBreakpointInternal);

  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.atomic_fence_inner", StdThreadingAtomicFenceInner);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.atomic_load_inner", StdThreadingAtomicLoadInner);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.atomic_store_inner", StdThreadingAtomicStoreInner);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.atomic_compex_inner", StdThreadingAtomicCompexInner);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.atomic_compex_weak_inner", StdThreadingAtomicCompexWeakInner);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.Atom.exchange", StdThreadingAtomicFetchExchange);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.Atom.fetch_and", StdThreadingAtomicFetchAnd);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.Atom.fetch_nand", StdThreadingAtomicFetchNand);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.Atom.fetch_or", StdThreadingAtomicFetchOr);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.Atom.fetch_xor", StdThreadingAtomicFetchXor);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.Atom.is_lock_free", StdThreadingAtomicIsLockFree);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.Atom.fetch_not", StdThreadingAtomicFetchNot);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.Atom.fetch_add", StdThreadingAtomicFetchAdd);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.Atom.fetch_sub", StdThreadingAtomicFetchSub);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.Atom.fetch_fadd", StdThreadingAtomicFetchFadd);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.Atom.fetch_fsub", StdThreadingAtomicFetchFsub);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.Atom.fetch_fmax", StdThreadingAtomicFetchFmax);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.Atom.fetch_fmin", StdThreadingAtomicFetchFmin);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.Atom.fetch_smax", StdThreadingAtomicFetchSmax);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.Atom.fetch_umax", StdThreadingAtomicFetchUmax);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.Atom.fetch_smin", StdThreadingAtomicFetchSmin);
  SPP_DEFINE_BUILTIN_FUNC("std.threading.atomic.Atom.fetch_umin", StdThreadingAtomicFetchUmin);
  return map;
}
