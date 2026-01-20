module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_func_impls;

import llvm;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts {
    SPP_EXP_CLS struct FunctionPrototypeAst;
    SPP_EXP_CLS struct TypeAst;
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
 */
export namespace spp::codegen::func_impls {
    template <typename F>
    auto simple_intrinsic_binop(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, F &&method) -> void;

    template <typename F>
    auto simple_intrinsic_binop_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, F &&method) -> void;

    template <typename F>
    auto simple_intrinsic_unop(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, F &&method) -> void;

    template <typename F>
    auto simple_intrinsic_unop_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, F &&method) -> void;

    auto simple_binary_intrinsic_call(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void;

    auto simple_unary_intrinsic_call(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void;

    auto std_abort_abort(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void;

    auto std_array_iter_mov(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_array_iter_mut(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_array_iter_ref(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_array_reverse_iter_mov(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_array_reverse_iter_mut(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_array_reverse_iter_ref(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_array_index_mut(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_array_index_ref(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_boolean_bit_and(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void;

    auto std_boolean_bit_ior(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void;

    auto std_boolean_bit_xor(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void;

    auto std_boolean_bit_not(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void;

    auto std_boolean_and(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void;

    auto std_boolean_ior(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void;

    auto std_boolean_eq(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void;

    auto std_boolean_ne(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void;

    auto std_cast_upcast_mov(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_cast_upcast_mut(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_cast_upcast_ref(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_cast_downcast_mov(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_cast_downcast_mut(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_cast_downcast_ref(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_generator_send(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_generator_once_send(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_memory_clear(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, std::shared_ptr<asts::TypeAst> const &spp_ty) -> void;

    auto std_memory_place_element(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, std::shared_ptr<asts::TypeAst> const &spp_ty) -> void;

    auto std_memory_take_element(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, std::shared_ptr<asts::TypeAst> const &spp_ty) -> void;

    auto std_memops_sizeof(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, std::shared_ptr<asts::TypeAst> const &spp_ty) -> void;

    // auto std_memops_sizeof_value(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void;

    auto std_intrinsics_add(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_add_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_sub(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_sub_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_mul(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_mul_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_sdiv(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_sdiv_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_udiv(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_udiv_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_srem(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_srem_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_urem(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_urem_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_sneg(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_bit_shl(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_bit_shl_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_bit_shr(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_bit_shr_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_bit_ior(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_bit_ior_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_bit_and(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_bit_and_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_bit_xor(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_bit_xor_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_bit_not(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_bit_not_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_abs(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

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

    auto std_intrinsics_min_val(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_max_val(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_smin(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_smax(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_umin(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_umax(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fadd(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fadd_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fsub(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fsub_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fmul(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fmul_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fdiv(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fdiv_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_frem(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_frem_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fneg(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fsqrt(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fpowi(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fpowf(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fsin(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fcos(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_ftan(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fasin(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_facos(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fatan(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fatan2(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fsinh(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fcosh(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_ftanh(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fexp(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fexp2(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fexp10(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fln(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_flog2(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_flog10(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fabs(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fmax_val(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fmin_val(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fmax(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fmin(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fcopysign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_ffloor(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fceil(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_ftrunc(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_fround(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_bitreverse(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_ctlz(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_sadd_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_uadd_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_ssub_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_usub_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_smul_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_umul_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_sadd_saturating(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_uadd_saturating(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_ssub_saturating(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_usub_saturating(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_sshl_saturating(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_ushl_saturating(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_sadd_wrapping(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_uadd_wrapping(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_ssub_wrapping(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_usub_wrapping(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_smul_wrapping(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_umul_wrapping(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

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

    auto std_intrinsics_fpclass(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
}
