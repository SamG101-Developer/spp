#pragma once
#include <string>

#include <spp/analyse/scopes/symbols.hpp>
#include <spp/codegen/llvm_ctx.hpp>


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
    template <typename F>
    auto simple_intrinsic_binop(LLvmCtx *ctx, llvm::Type *ty, std::string const &op_name, F &&method) -> void {
        // Create the binary function.
        const auto fn_ty = llvm::FunctionType::get(ty, {ty, ty}, false);
        const auto fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, op_name, ctx->module.get());
        const auto entry_bb = llvm::BasicBlock::Create(ctx->context, "entry", fn);

        // Build the function body.
        ctx->builder.SetInsertPoint(entry_bb);
        const auto lhs = fn->arg_begin();
        const auto rhs = fn->arg_begin() + 1;
        const auto result = method(lhs, rhs);
        ctx->builder.CreateRet(result);
    }

    template <typename F>
    auto simple_intrinsic_binop_assign(LLvmCtx *ctx, llvm::Type *ty, std::string const &op_name, F &&method) -> void {
        // Create the add_assign function.
        const auto ptr_ty = llvm::PointerType::get(ctx->context, 0);
        const auto fn_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx->context), {ptr_ty, ty}, false);
        const auto fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, op_name, ctx->module.get());
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

    template <typename F>
    auto simple_intrinsic_unop(LLvmCtx *ctx, llvm::Type *ty, std::string const &op_name, F &&method) -> void {
        // Create the unary function.
        const auto fn_ty = llvm::FunctionType::get(ty, {ty}, false);
        const auto fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, op_name, ctx->module.get());
        const auto entry_bb = llvm::BasicBlock::Create(ctx->context, "entry", fn);

        // Build the function body.
        ctx->builder.SetInsertPoint(entry_bb);
        const auto operand = fn->arg_begin();
        const auto result = method(operand);
        ctx->builder.CreateRet(result);
    }

    inline auto simple_binary_intrinsic_call(LLvmCtx *ctx, llvm::Type *ty, std::string const &intrinsic_name, const llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void {
        // Create the function.
        const auto fn_ty = llvm::FunctionType::get(ty, {ty, ty}, false);
        const auto fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, intrinsic_name, ctx->module.get());
        const auto entry_bb = llvm::BasicBlock::Create(ctx->context, "entry", fn);

        // Build the function body.
        ctx->builder.SetInsertPoint(entry_bb);
        const auto lhs = fn->arg_begin();
        const auto rhs = fn->arg_begin() + 1;
        const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(ctx->module.get(), intrinsic, {ty});
        const auto result = ctx->builder.CreateCall(intrinsic_fn, {lhs, rhs}, "result");
        ctx->builder.CreateRet(result);
    }

    inline auto simple_unary_intrinsic_call(LLvmCtx *ctx, llvm::Type *ty, std::string const &intrinsic_name, const llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void {
        // Create the function.
        const auto fn_ty = llvm::FunctionType::get(ty, {ty}, false);
        const auto fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, intrinsic_name, ctx->module.get());
        const auto entry_bb = llvm::BasicBlock::Create(ctx->context, "entry", fn);

        // Build the function body.
        ctx->builder.SetInsertPoint(entry_bb);
        const auto operand = fn->arg_begin();
        const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(ctx->module.get(), intrinsic, {ty});
        const auto result = ctx->builder.CreateCall(intrinsic_fn, {operand}, "result");
        ctx->builder.CreateRet(result);
    }

    auto std_abort_abort(LLvmCtx *ctx) -> void;

    auto std_array_iter_mov(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_array_iter_mut(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_array_iter_ref(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_array_reverse_iter_mov(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_array_reverse_iter_mut(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_array_reverse_iter_ref(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_array_index_mut(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_array_index_ref(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_boolean_bit_and(LLvmCtx *ctx) -> void;

    auto std_boolean_bit_ior(LLvmCtx *ctx) -> void;

    auto std_boolean_bit_xor(LLvmCtx *ctx) -> void;

    auto std_boolean_and(LLvmCtx *ctx) -> void;

    auto std_boolean_ior(LLvmCtx *ctx) -> void;

    auto std_boolean_not(LLvmCtx *ctx) -> void;

    auto std_boolean_eq(LLvmCtx *ctx) -> void;

    auto std_boolean_ne(LLvmCtx *ctx) -> void;

    auto std_cast_upcast_mov(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_cast_upcast_mut(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_cast_upcast_ref(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_cast_downcast_mov(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_cast_downcast_mut(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_cast_downcast_ref(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_generator_send(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_generator_opt_send(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_generator_res_send(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_generator_once_send(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_add(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_add_assign(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_sub(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsics_sub_assign(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_mul(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_mul_assign(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sdiv(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sdiv_assign(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_udiv(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_udiv_assign(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_srem(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_srem_assign(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_urem(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_urem_assign(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sneg(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_shl(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_shl_assign(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_shr(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_shr_assign(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_ior(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_ior_assign(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_and(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_and_assign(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_xor(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_bit_xor_assign(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_abs(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_eq(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_oeq(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ne(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_one(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_slt(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ult(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_olt(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sle(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ule(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ole(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sgt(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ugt(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ogt(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sge(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_uge(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_oge(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_max_val(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_min_val(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_smin(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_smax(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_umin(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_umax(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fadd(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fadd_assign(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fsub(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fsub_assign(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fmul(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fmul_assign(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fdiv(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fdiv_assign(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_frem(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_frem_assign(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fneg(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fsqrt(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fcbrt(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fpowi(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fpowf(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fsin(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fcos(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ftan(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fasin(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_facos(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fatan(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fatan2(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fsinh(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fcosh(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ftanh(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fexp(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fexp2(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fexp10(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fln(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_flog2(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_flog10(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fabs(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fmax(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fmin(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fcopysign(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ffloor(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fceil(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ftrunc(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fround(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fbitreverse(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ctlz(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sadd_overflow(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_uadd_overflow(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ssub_overflow(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_usub_overflow(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_smul_overflow(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_umul_overflow(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sadd_saturating(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_uadd_saturating(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ssub_saturating(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_usub_saturating(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sshl_saturating(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ushl_saturating(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sadd_wrapping(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_uadd_wrapping(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_ssub_wrapping(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_usub_wrapping(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_smul_wrapping(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_umul_wrapping(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_sitofp(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_uitofp(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fptrunc(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_strunc(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_utrunc(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_szext(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_uzext(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fpext(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fptosi(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fptoui(LLvmCtx *ctx, llvm::Type *ty) -> void;

    auto std_intrinsic_fpclass(LLvmCtx *ctx, llvm::Type *ty) -> void;
}
