module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_func_impls;
import spp.asts._fwd;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_mangle;
import spp.analyse.scopes.scope_manager;

import llvm;
import std;


#define SPP_LLVM_FUNC_INFO analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto


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
namespace spp::codegen::func_impls {
    SPP_EXP_CLS template <typename F>
    auto simple_intrinsic_binop(
        SPP_LLVM_FUNC_INFO,
        LLvmCtx *ctx,
        llvm::Type *ty,
        F &&method)
        -> void {
        // Create the binary function.
        const auto name = mangle::mangle_fun_name(*sm->current_scope, *proto);
        const auto fn_ty = llvm::FunctionType::get(ty, {ty, ty}, false);
        const auto fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, name, ctx->module.get());
        const auto entry_bb = llvm::BasicBlock::Create(ctx->context, "entry", fn);

        // Build the function body.
        ctx->builder.SetInsertPoint(entry_bb);
        const auto lhs = fn->arg_begin();
        const auto rhs = fn->arg_begin() + 1;
        const auto result = method(lhs, rhs);
        ctx->builder.CreateRet(result);
    }

    SPP_EXP_CLS template <typename F>
    auto simple_intrinsic_binop_assign(
        SPP_LLVM_FUNC_INFO,
        LLvmCtx *ctx,
        llvm::Type *ty,
        F &&method)
        -> void {
        // Create the add_assign function.
        const auto name = mangle::mangle_fun_name(*sm->current_scope, *proto);
        const auto ptr_ty = llvm::PointerType::get(ctx->context, 0);
        const auto fn_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx->context), {ptr_ty, ty}, false);
        const auto fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, name, ctx->module.get());
        const auto entry_bb = llvm::BasicBlock::Create(ctx->context, "entry", fn);

        // Build the function body.
        ctx->builder.SetInsertPoint(entry_bb);
        const auto lhs = fn->arg_begin();
        const auto rhs = fn->arg_begin() + 1;
        const auto loaded_val = ctx->builder.CreateLoad(ty, lhs, "loaded_val");
        const auto result = method(loaded_val, rhs);
        ctx->builder.CreateStore(result, lhs);
        ctx->builder.CreateRetVoid();
    }

    SPP_EXP_CLS template <typename F>
    auto simple_intrinsic_unop(
        SPP_LLVM_FUNC_INFO,
        LLvmCtx *ctx,
        llvm::Type *ty,
        F &&method)
        -> void {
        // Create the unary function.
        const auto name = mangle::mangle_fun_name(*sm->current_scope, *proto);
        const auto fn_ty = llvm::FunctionType::get(ty, {ty}, false);
        const auto fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, name, ctx->module.get());
        const auto entry_bb = llvm::BasicBlock::Create(ctx->context, "entry", fn);

        // Build the function body.
        ctx->builder.SetInsertPoint(entry_bb);
        const auto operand = fn->arg_begin();
        const auto result = method(operand);
        ctx->builder.CreateRet(result);
    }

    auto simple_binary_intrinsic_call(
        SPP_LLVM_FUNC_INFO,
        LLvmCtx *ctx,
        llvm::Type *ty,
        const llvm::Intrinsic::IndependentIntrinsics intrinsic)
        -> void {
        // Create the function.
        const auto name = mangle::mangle_fun_name(*sm->current_scope, *proto);
        const auto fn_ty = llvm::FunctionType::get(ty, {ty, ty}, false);
        const auto fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, name, ctx->module.get());
        const auto entry_bb = llvm::BasicBlock::Create(ctx->context, "entry", fn);

        // Build the function body.
        ctx->builder.SetInsertPoint(entry_bb);
        const auto lhs = fn->arg_begin();
        const auto rhs = fn->arg_begin() + 1;
        const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(ctx->module.get(), intrinsic, {ty});
        const auto result = ctx->builder.CreateCall(intrinsic_fn, {lhs, rhs}, "result");
        ctx->builder.CreateRet(result);
    }

    auto simple_unary_intrinsic_call(
        SPP_LLVM_FUNC_INFO,
        LLvmCtx *ctx,
        llvm::Type *ty,
        const llvm::Intrinsic::IndependentIntrinsics intrinsic)
        -> void {
        // Create the function.
        const auto name = mangle::mangle_fun_name(*sm->current_scope, *proto);
        const auto fn_ty = llvm::FunctionType::get(ty, {ty}, false);
        const auto fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, name, ctx->module.get());
        const auto entry_bb = llvm::BasicBlock::Create(ctx->context, "entry", fn);

        // Build the function body.
        ctx->builder.SetInsertPoint(entry_bb);
        const auto operand = fn->arg_begin();
        const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(ctx->module.get(), intrinsic, {ty});
        const auto result = ctx->builder.CreateCall(intrinsic_fn, {operand}, "result");
        ctx->builder.CreateRet(result);
    }

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

    auto std_boolean_and(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void;

    auto std_boolean_ior(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void;

    auto std_boolean_not(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void;

    auto std_boolean_eq(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void;

    auto std_boolean_ne(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void;

    auto std_cast_upcast_mov(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_cast_upcast_mut(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_cast_upcast_ref(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_cast_downcast_mov(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_cast_downcast_mut(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_cast_downcast_ref(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_generator_send(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_generator_opt_send(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_generator_res_send(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

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

    auto std_intrinsic_mul(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_mul_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sdiv(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sdiv_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_udiv(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_udiv_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_srem(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_srem_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_urem(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_urem_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sneg(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_shl(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_shl_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_shr(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_shr_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_ior(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_ior_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_and(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_and_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_xor(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_xor_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_abs(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_eq(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_oeq(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ne(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_one(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_slt(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ult(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_olt(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sle(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ule(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ole(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sgt(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ugt(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ogt(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sge(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_uge(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_oge(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_max_val(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_min_val(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_smin(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_smax(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_umin(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_umax(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fadd(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fadd_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fsub(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fsub_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fmul(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fmul_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fdiv(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fdiv_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_frem(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_frem_assign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fneg(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fsqrt(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fcbrt(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fpowi(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fpowf(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fsin(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fcos(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ftan(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fasin(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_facos(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fatan(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fatan2(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fsinh(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fcosh(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ftanh(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fexp(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fexp2(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fexp10(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fln(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_flog2(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_flog10(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fabs(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fmax(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fmin(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fcopysign(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ffloor(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fceil(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ftrunc(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fround(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fbitreverse(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ctlz(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sadd_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_uadd_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ssub_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_usub_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_smul_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_umul_overflow(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sadd_saturating(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_uadd_saturating(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ssub_saturating(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_usub_saturating(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sshl_saturating(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ushl_saturating(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sadd_wrapping(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_uadd_wrapping(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ssub_wrapping(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_usub_wrapping(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_smul_wrapping(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_umul_wrapping(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sitofp(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_uitofp(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fptrunc(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_strunc(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_utrunc(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_szext(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_uzext(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fpext(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fptosi(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fptoui(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fpclass(SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void;
}
