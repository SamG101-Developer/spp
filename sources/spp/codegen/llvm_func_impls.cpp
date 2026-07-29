module;
#include <spp/macros.hpp>

module spp.codegen.llvm_func_impls;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.coroutine_prototype_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.function_prototype_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_func;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_mangle;
import spp.codegen.llvm_size;
import spp.codegen.llvm_type;
import spp.utils.uid;
import llvm;
import std;

// =========================================================================================================
// Layer 1: function + entry-block creation.
// =========================================================================================================

auto spp::codegen::func_impls::simple_create_fn(
  SPP_LLVM_FUNC_INFO,
  LLvmCtx *ctx,
  llvm::Type *ret_ty,
  Vec<llvm::Type*> const &param_tys)
  -> llvm::Function* {
  const auto uid = "." + spp::utils::Uid();
  const auto name = mangle::mangle_fun_name(*sm->CurrentScope, *proto);
  const auto fn_ty = llvm::FunctionType::get(ret_ty, param_tys.ToStdVector(), false);
  const auto fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, name, ctx->Module.get());
  const auto entry_bb = llvm::BasicBlock::Create(*ctx->Context, "entry" + uid, fn);
  ctx->Builder.SetInsertPoint(entry_bb);
  return fn;
}

// =========================================================================================================
// Layer 2: enum-driven operation dispatchers.
// =========================================================================================================

auto spp::codegen::func_impls::is_cmp_bin_op(BinOp op) -> bool {
  switch (op) {
    case BinOp::ICmpEQ:
    case BinOp::ICmpNE:
    case BinOp::ICmpSLT:
    case BinOp::ICmpULT:
    case BinOp::ICmpSLE:
    case BinOp::ICmpULE:
    case BinOp::ICmpSGT:
    case BinOp::ICmpUGT:
    case BinOp::ICmpSGE:
    case BinOp::ICmpUGE:
    case BinOp::FCmpOEQ:
    case BinOp::FCmpONE:
    case BinOp::FCmpOLT:
    case BinOp::FCmpOLE:
    case BinOp::FCmpOGT:
    case BinOp::FCmpOGE:
      return true;
    default:
      return false;
  }
}

auto spp::codegen::func_impls::apply_bin_op(
  LLvmCtx *ctx, BinOp op, llvm::Value *a, llvm::Value *b)
  -> llvm::Value* {
  const auto name = "result" + spp::utils::Uid();
  switch (op) {
    case BinOp::Add: return ctx->Builder.CreateAdd(a, b, name);
    case BinOp::Sub: return ctx->Builder.CreateSub(a, b, name);
    case BinOp::Mul: return ctx->Builder.CreateMul(a, b, name);
    case BinOp::SDiv: return ctx->Builder.CreateSDiv(a, b, name);
    case BinOp::UDiv: return ctx->Builder.CreateUDiv(a, b, name);
    case BinOp::SRem: return ctx->Builder.CreateSRem(a, b, name);
    case BinOp::URem: return ctx->Builder.CreateURem(a, b, name);
    case BinOp::Shl: return ctx->Builder.CreateShl(a, b, name);
    case BinOp::LShr: return ctx->Builder.CreateLShr(a, b, name);
    case BinOp::Or: return ctx->Builder.CreateOr(a, b, name);
    case BinOp::And: return ctx->Builder.CreateAnd(a, b, name);
    case BinOp::Xor: return ctx->Builder.CreateXor(a, b, name);
    case BinOp::ICmpEQ: return ctx->Builder.CreateICmpEQ(a, b, name);
    case BinOp::ICmpNE: return ctx->Builder.CreateICmpNE(a, b, name);
    case BinOp::ICmpSLT: return ctx->Builder.CreateICmpSLT(a, b, name);
    case BinOp::ICmpULT: return ctx->Builder.CreateICmpULT(a, b, name);
    case BinOp::ICmpSLE: return ctx->Builder.CreateICmpSLE(a, b, name);
    case BinOp::ICmpULE: return ctx->Builder.CreateICmpULE(a, b, name);
    case BinOp::ICmpSGT: return ctx->Builder.CreateICmpSGT(a, b, name);
    case BinOp::ICmpUGT: return ctx->Builder.CreateICmpUGT(a, b, name);
    case BinOp::ICmpSGE: return ctx->Builder.CreateICmpSGE(a, b, name);
    case BinOp::ICmpUGE: return ctx->Builder.CreateICmpUGE(a, b, name);
    case BinOp::FCmpOEQ: return ctx->Builder.CreateFCmpOEQ(a, b, name);
    case BinOp::FCmpONE: return ctx->Builder.CreateFCmpONE(a, b, name);
    case BinOp::FCmpOLT: return ctx->Builder.CreateFCmpOLT(a, b, name);
    case BinOp::FCmpOLE: return ctx->Builder.CreateFCmpOLE(a, b, name);
    case BinOp::FCmpOGT: return ctx->Builder.CreateFCmpOGT(a, b, name);
    case BinOp::FCmpOGE: return ctx->Builder.CreateFCmpOGE(a, b, name);
    case BinOp::FAdd: return ctx->Builder.CreateFAdd(a, b, name);
    case BinOp::FSub: return ctx->Builder.CreateFSub(a, b, name);
    case BinOp::FMul: return ctx->Builder.CreateFMul(a, b, name);
    case BinOp::FDiv: return ctx->Builder.CreateFDiv(a, b, name);
    case BinOp::FRem: return ctx->Builder.CreateFRem(a, b, name);
    case BinOp::NSWAdd: return ctx->Builder.CreateNSWAdd(a, b, name);
    case BinOp::NUWAdd: return ctx->Builder.CreateNUWAdd(a, b, name);
    case BinOp::NSWSub: return ctx->Builder.CreateNSWSub(a, b, name);
    case BinOp::NUWSub: return ctx->Builder.CreateNUWSub(a, b, name);
    case BinOp::NSWMul: return ctx->Builder.CreateNSWMul(a, b, name);
    case BinOp::NUWMul: return ctx->Builder.CreateNUWMul(a, b, name);
    case BinOp::LogicalAnd: return ctx->Builder.CreateLogicalAnd(a, b, name);
    case BinOp::LogicalOr: return ctx->Builder.CreateLogicalOr(a, b, name);
    default: throw std::runtime_error(std::format("Unsupported BinOp type: {}", name));
  }
  SPP_ASSERT(false);
  return nullptr;
}

auto spp::codegen::func_impls::apply_un_op(
  LLvmCtx *ctx, const UnOp op, llvm::Value *a)
  -> llvm::Value* {
  const auto name = "result" + spp::utils::Uid();
  switch (op) {
    case UnOp::Neg: return ctx->Builder.CreateNeg(a, name);
    case UnOp::Not: return ctx->Builder.CreateNot(a, name);
    case UnOp::FNeg: return ctx->Builder.CreateFNeg(a, name);
    default: throw std::runtime_error(std::format("Unsupported UnOp type: {}", name));
  }
  SPP_ASSERT(false);
  return nullptr;
}

auto spp::codegen::func_impls::apply_conv_op(
  LLvmCtx *ctx, const ConvOp op, llvm::Value *a, llvm::Type *dest_ty)
  -> llvm::Value* {
  const auto name = "result" + spp::utils::Uid();
  switch (op) {
    case ConvOp::SIToFP: return ctx->Builder.CreateSIToFP(a, dest_ty, name);
    case ConvOp::UIToFP: return ctx->Builder.CreateUIToFP(a, dest_ty, name);
    case ConvOp::FPTrunc: return ctx->Builder.CreateFPTrunc(a, dest_ty, name);
    case ConvOp::Trunc: return ctx->Builder.CreateTrunc(a, dest_ty, name);
    case ConvOp::SExt: return ctx->Builder.CreateSExt(a, dest_ty, name);
    case ConvOp::ZExt: return ctx->Builder.CreateZExt(a, dest_ty, name);
    case ConvOp::FPExt: return ctx->Builder.CreateFPExt(a, dest_ty, name);
    case ConvOp::BitCast: return ctx->Builder.CreateBitCast(a, dest_ty, name);
    case ConvOp::FPToSI: return ctx->Builder.CreateFPToSI(a, dest_ty, name);
    case ConvOp::FPToUI: return ctx->Builder.CreateFPToUI(a, dest_ty, name);
    default: throw std::runtime_error(std::format("Unsupported ConvOp type: {}", name));
  }
  SPP_ASSERT(false);
  return nullptr;
}

auto spp::codegen::func_impls::simple_intrinsic_binop(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, const BinOp op)
  -> void {
  // "ty" (per the dispatcher) is always the function's declared RETURN type. For arithmetic ops that's also the
  // operand type ("T, T -> T"). For comparisons the return type is "Bool" (i1), so the *operand* type has to be read
  // off the function's own first parameter instead - "ty" alone can't give us both.
  const auto operand_ty = is_cmp_bin_op(op)
    ? GetLlvmType(*sm->CurrentScope->GetTypeSymbol(
                    sm->CurrentScope->GetVarSymbol(proto->FnParamGroup->GetAllParams()[0]->ExtractName().get())->Type.
                        get()), ctx)
    : ty;
  const auto fn = simple_create_fn(sm, proto, ctx, ty, Vec{operand_ty, operand_ty});
  const auto lhs = fn->arg_begin();
  const auto rhs = fn->arg_begin() + 1;
  ctx->Builder.CreateRet(apply_bin_op(ctx, op, lhs, rhs));
}

auto spp::codegen::func_impls::simple_intrinsic_binop_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *, const BinOp op)
  -> void {
  // "(this: &mut T, that: T) -> Void": "ty" (per the dispatcher) is the declared return type "Void", not "T" - the
  // operand type is read off "that" (the last parameter) instead, which - unlike "this" - is a plain "T" rather than
  // a reference, so there's no reference-unwrapping ambiguity.
  const auto uid = "." + spp::utils::Uid();
  const auto that_param = proto->FnParamGroup->GetAllParams().Back();
  const auto that_sym = sm->CurrentScope->GetVarSymbol(that_param->ExtractName().get());
  const auto operand_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(that_sym->Type.get()), ctx);

  const auto void_ty = llvm::Type::getVoidTy(*ctx->Context);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto fn = simple_create_fn(sm, proto, ctx, void_ty, Vec<llvm::Type*>{ptr_ty, operand_ty});

  const auto lhs = fn->arg_begin();
  const auto rhs = fn->arg_begin() + 1;
  const auto loaded_val = ctx->Builder.CreateLoad(operand_ty, lhs, "intrinsic.assign.loaded" + uid);
  const auto result = apply_bin_op(ctx, op, loaded_val, rhs);
  ctx->Builder.CreateStore(result, lhs);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::simple_intrinsic_unop(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, const UnOp op)
  -> void {
  const auto fn = simple_create_fn(sm, proto, ctx, ty, Vec{ty});
  const auto operand = fn->arg_begin();
  ctx->Builder.CreateRet(apply_un_op(ctx, op, operand));
}

auto spp::codegen::func_impls::simple_intrinsic_unop_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *, const UnOp op)
  -> void {
  // "(this: &mut T) -> Void": "ty" (per the dispatcher) is the declared return type "Void", not "T" - the operand
  // type is read off "this" instead. "this" is itself a "&mut T" reference, but (matching how every other
  // reference-typed symbol in this file - e.g. "self" - is resolved) GetTypeSymbol/GetLlvmType already unwraps the
  // reference down to plain "T", not a raw pointer type.
  const auto uid = "." + spp::utils::Uid();
  const auto this_param = proto->FnParamGroup->GetAllParams()[0];
  const auto this_sym = sm->CurrentScope->GetVarSymbol(this_param->ExtractName().get());
  const auto operand_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(this_sym->Type.get()), ctx);

  const auto void_ty = llvm::Type::getVoidTy(*ctx->Context);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto fn = simple_create_fn(sm, proto, ctx, void_ty, Vec<llvm::Type*>{ptr_ty});

  const auto lhs = fn->arg_begin();
  const auto loaded_val = ctx->Builder.CreateLoad(operand_ty, lhs, "intrinsic.assign.loaded" + uid);
  const auto result = apply_un_op(ctx, op, loaded_val);
  ctx->Builder.CreateStore(result, lhs);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::simple_intrinsic_conv(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, const ConvOp op)
  -> void {
  // "ty" (per the dispatcher) is the function's declared RETURN type - the conversion's destination. The source
  // (operand) type is read off the function's own single parameter instead, since conversions genuinely go from one
  // type to a different one (e.g. "S32 -> F64"), unlike every other builder here where operand type == return type.
  const auto param = proto->FnParamGroup->GetAllParams()[0];
  const auto param_sym = sm->CurrentScope->GetVarSymbol(param->ExtractName().get());
  const auto src_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(param_sym->Type.get()), ctx);
  const auto fn = simple_create_fn(sm, proto, ctx, ty, Vec<llvm::Type*>{src_ty});
  const auto operand = fn->arg_begin();
  ctx->Builder.CreateRet(apply_conv_op(ctx, op, operand, ty));
}

auto spp::codegen::func_impls::simple_intrinsic_is_const(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, const bool is_float, const double value)
  -> void {
  // "ty" (per the dispatcher) is the declared return type, "Bool" (i1) here - the operand's real type (T) is read
  // off "self" instead ("is_zero(&self) -> Bool" et al).
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto operand_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(self_sym->Type.get()), ctx);
  const auto fn = simple_create_fn(sm, proto, ctx, ty, Vec<llvm::Type*>{operand_ty});
  const auto operand = fn->arg_begin();
  const auto name = "result" + spp::utils::Uid();
  const auto result = is_float
    ? ctx->Builder.CreateFCmpOEQ(operand, llvm::ConstantFP::get(operand_ty, value), name)
    : ctx->Builder.CreateICmpEQ(operand, llvm::ConstantInt::get(operand_ty, static_cast<std::uint64_t>(value)), name);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::func_impls::apply_atomic_rmw_op(AtomicRmwOp op) -> llvm::AtomicRMWInst::BinOp {
  switch (op) {
    case AtomicRmwOp::Xchg: return llvm::AtomicRMWInst::Xchg;
    case AtomicRmwOp::Add: return llvm::AtomicRMWInst::Add;
    case AtomicRmwOp::Sub: return llvm::AtomicRMWInst::Sub;
    case AtomicRmwOp::And: return llvm::AtomicRMWInst::And;
    case AtomicRmwOp::Nand: return llvm::AtomicRMWInst::Nand;
    case AtomicRmwOp::Or: return llvm::AtomicRMWInst::Or;
    case AtomicRmwOp::Xor: return llvm::AtomicRMWInst::Xor;
    case AtomicRmwOp::Max: return llvm::AtomicRMWInst::Max;
    case AtomicRmwOp::Min: return llvm::AtomicRMWInst::Min;
    case AtomicRmwOp::UMax: return llvm::AtomicRMWInst::UMax;
    case AtomicRmwOp::UMin: return llvm::AtomicRMWInst::UMin;
    case AtomicRmwOp::FAdd: return llvm::AtomicRMWInst::FAdd;
    case AtomicRmwOp::FSub: return llvm::AtomicRMWInst::FSub;
    case AtomicRmwOp::FMax: return llvm::AtomicRMWInst::FMax;
    case AtomicRmwOp::FMin: return llvm::AtomicRMWInst::FMin;
    default: throw std::runtime_error(std::format("Unsupported AtomicRmwOp type: {}", static_cast<int>(op)));
  }
  SPP_ASSERT(false);
  return llvm::AtomicRMWInst::Xchg;
}

auto spp::codegen::func_impls::simple_atomic_fetch_rmw(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx, const AtomicRmwOp op)
  -> void {
  // "(&self, val: T, order: U8) -> T": a plain method (not a coroutine, and not a free "_inner" function), so its
  // "llvm::Function" is already declared/opened by the time this runs - same as "std_slot_replace" - and "self" is
  // already bound; no "simple_create_fn"/env indirection needed.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  // "self" is "&Atom[T]" - a borrow, so its frame slot holds the *address* of the caller's "Atom[T]" instance, not
  // the instance itself; that address has to be loaded out before it can be used as a GEP base (see
  // "PostfixExpressionOperatorRuntimeMemberAccessAst::Stage11_CodeGen"'s "is_borrow" handling for the same rule).
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "atomic.fetch.self");
  const auto atom_ty = llvm::cast<llvm::StructType>(
    GetLlvmType(*sm->CurrentScope->GetTypeSymbol(self_sym->Type.get()), ctx));
  const auto val_field_ptr = ctx->Builder.CreateStructGEP(atom_ty, self_ptr, 0, "atomic.fetch.val_ptr");
  const auto val_ty = atom_ty->getElementType(0);

  // "val" and "order" are the two non-"self" parameters.
  const auto val_param = proto->FnParamGroup->GetAllParams()[0];
  const auto val_sym = sm->CurrentScope->GetVarSymbol(val_param->ExtractName().get());
  const auto val_arg = ctx->Builder.CreateLoad(val_ty, val_sym->LlvmInfo->Alloca, "atomic.fetch.operand");

  const auto order_param = proto->FnParamGroup->GetAllParams()[1];
  const auto order_sym = sm->CurrentScope->GetVarSymbol(order_param->ExtractName().get());
  const auto order_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto order_arg = llvm::cast<llvm::ConstantInt>(
    ctx->Builder.CreateLoad(order_ty, order_sym->LlvmInfo->Alloca, "atomic.fetch.order"));

  auto const &dl = ctx->Module->getDataLayout();
  const auto rmw_inst = ctx->Builder.CreateAtomicRMW(
    apply_atomic_rmw_op(op), val_field_ptr, val_arg, dl.getABITypeAlign(val_ty),
    static_cast<llvm::AtomicOrdering>(order_arg->getZExtValue()));
  ctx->Builder.CreateRet(rmw_inst);
}

auto spp::codegen::func_impls::simple_binary_intrinsic_call(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, const llvm::Intrinsic::IndependentIntrinsics intrinsic)
  -> void {
  const auto uid = "." + spp::utils::Uid();
  const auto fn = simple_create_fn(sm, proto, ctx, ty, Vec{ty, ty});
  const auto lhs = fn->arg_begin();
  const auto rhs = fn->arg_begin() + 1;
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(ctx->Module.get(), intrinsic, {ty});
  const auto result = ctx->Builder.CreateCall(intrinsic_fn, {lhs, rhs}, "intrinsic.result" + uid);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::func_impls::simple_binary_intrinsic_call_overflow(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, const llvm::Intrinsic::IndependentIntrinsics intrinsic)
  -> void {
  // "ty" (per the dispatcher) is already the whole return type's own lowering - "(T, Bool)" is a literal struct, so
  // "ty" arrives as exactly "{T, i1}". "T" (the operand type "llvm.sadd.with.overflow" etc. actually take) is pulled
  // back out of that struct's first field, rather than needing a separate parameter lookup.
  const auto uid = "." + spp::utils::Uid();
  const auto ret_ty = llvm::cast<llvm::StructType>(ty);
  const auto elem_ty = ret_ty->getElementType(0);
  const auto fn = simple_create_fn(sm, proto, ctx, ret_ty, Vec<llvm::Type*>{elem_ty, elem_ty});
  const auto lhs = fn->arg_begin();
  const auto rhs = fn->arg_begin() + 1;
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(ctx->Module.get(), intrinsic, {elem_ty});
  const auto result = ctx->Builder.CreateCall(intrinsic_fn, {lhs, rhs}, "intrinsic.result" + uid);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::func_impls::simple_unary_intrinsic_call(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, const llvm::Intrinsic::IndependentIntrinsics intrinsic)
  -> void {
  const auto uid = "." + spp::utils::Uid();
  const auto fn = simple_create_fn(sm, proto, ctx, ty, Vec{ty});
  const auto operand = fn->arg_begin();
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(ctx->Module.get(), intrinsic, {ty});
  const auto result = ctx->Builder.CreateCall(intrinsic_fn, {operand}, "intrinsic.result" + uid);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::func_impls::simple_get_value(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, llvm::Value *val)
  -> void {
  const auto fn = simple_create_fn(sm, proto, ctx, ty, Vec{ty});
  (void)fn;
  ctx->Builder.CreateRet(val);
}

// =========================================================================================================
// Layer 2b: coroutine-specific shared helpers.
// =========================================================================================================

auto spp::codegen::func_impls::simple_coro_array_iter_mov(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  const bool reverse)
  -> void {
  // "self" (the "Arr[T, n]", consumed by value) already lives inline in the coroutine's env frame - its storage is
  // the frame field itself, not a separate heap buffer, since arrays have no allocator.
  const auto coro = proto->To<asts::CoroutinePrototypeAst>();
  SPP_ASSERT(coro != nullptr);
  const auto env_type = coro->LlvmCoroGenEnvType;
  const auto env_ptr = coro->LlvmCoroGenEnv;

  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto self_ptr = self_sym->LlvmInfo->Alloca;
  const auto arr_ty = llvm::cast<llvm::ArrayType>(
    GetLlvmType(*sm->CurrentScope->GetTypeSymbol(self_sym->Type.get()), ctx));
  const auto elem_ty = arr_ty->getElementType();
  const auto n = arr_ty->getNumElements();

  const auto i8_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);

  // The array's length is a compile-time constant, so this unrolls into one "gen"-style yield per element (see
  // "GenExpressionAst::Stage11_CodeGen" for the hand-written equivalent) rather than a runtime loop - pointer
  // arithmetic like this cannot be expressed in safe S++ anyway.
  for (auto i = 0uz; i < n; ++i) {
    const auto idx = reverse ? n - 1 - i : i;
    const auto uid = "." + spp::utils::Uid();

    // Move "self[idx]" out into the yield slot.
    const auto idx0 = llvm::ConstantInt::get(*ctx->Context, llvm::APInt(64, 0));
    const auto idx1 = llvm::ConstantInt::get(*ctx->Context, llvm::APInt(64, idx));
    const auto elem_ptr = ctx->Builder.CreateGEP(arr_ty, self_ptr, {idx0, idx1}, "arr.iter.elem" + uid);
    const auto elem_val = ctx->Builder.CreateLoad(elem_ty, elem_ptr, "arr.iter.val" + uid);

    // Suspend with the value: mark YIELDED and record the resume location (mirrors GenExpressionAst).
    const auto location = ctx->YieldContinuations.Len() + 1;
    ctx->Builder.CreateStore(
      llvm::ConstantInt::get(i8_ty, std::to_underlying(CoroutineState::YIELDED)),
      ctx->Builder.CreateStructGEP(env_type, env_ptr, std::to_underlying(GenEnvField::STATE), "arr.iter.state" + uid));
    ctx->Builder.CreateStore(
      llvm::ConstantInt::get(i32_ty, location),
      ctx->Builder.CreateStructGEP(env_type, env_ptr, std::to_underlying(GenEnvField::LOCATION), "arr.iter.loc" + uid));
    ctx->Builder.CreateStore(
      elem_val,
      ctx->Builder.CreateStructGEP(
        env_type, env_ptr, std::to_underlying(GenEnvField::YIELD_SLOT), "arr.iter.yield" + uid));
    ctx->Builder.CreateRetVoid();

    // The continuation block: where the next ".res()" resumes to. Registering it lets
    // "CoroutinePrototypeAst::Stage11_CodeGen" wire it into the resume switch, exactly as a real "gen" would.
    const auto cont_bb = llvm::BasicBlock::Create(
      *ctx->Context, "arr.iter.cont" + uid, ctx->Builder.GetInsertBlock()->getParent());
    ctx->YieldContinuations.push_back(cont_bb);
    ctx->Builder.SetInsertPoint(cont_bb);
  }

  // Falling out of the final continuation with no terminator is exactly what a coroutine body running off the end
  // after its last "gen" looks like; the caller marks the state EXHAUSTED and returns.
}

auto spp::codegen::func_impls::simple_coro_slot_get(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx)
  -> void {
  const auto coro = proto->To<asts::CoroutinePrototypeAst>();
  SPP_ASSERT(coro != nullptr);
  const auto env_type = coro->LlvmCoroGenEnvType;
  const auto env_ptr = coro->LlvmCoroGenEnv;

  // "self" is "&Slot[T]" - a borrow, so its frame field holds the *address* of the caller's "Slot[T]" instance, not
  // the instance itself; that address has to be loaded out before it can be used as a GEP base (see
  // "PostfixExpressionOperatorRuntimeMemberAccessAst::Stage11_CodeGen"'s "is_borrow" handling for the same rule).
  // "val" is field 0 (Slot's only field), and its address is what gets yielded (as "&T"/"&mut T").
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "slot.get.self");
  const auto slot_ty = llvm::cast<llvm::StructType>(
    GetLlvmType(*sm->CurrentScope->GetTypeSymbol(self_sym->Type.get()), ctx));
  const auto val_ptr = ctx->Builder.CreateStructGEP(slot_ty, self_ptr, 0, "slot.get.val_ptr");

  const auto uid = "." + spp::utils::Uid();
  const auto i8_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);

  // Suspend once (mirrors GenExpressionAst): mark YIELDED, record the resume location, and store the address of
  // "self.val" into the yield slot.
  const auto location = ctx->YieldContinuations.Len() + 1;
  ctx->Builder.CreateStore(
    llvm::ConstantInt::get(i8_ty, std::to_underlying(CoroutineState::YIELDED)),
    ctx->Builder.CreateStructGEP(env_type, env_ptr, std::to_underlying(GenEnvField::STATE), "slot.get.state" + uid));
  ctx->Builder.CreateStore(
    llvm::ConstantInt::get(i32_ty, location),
    ctx->Builder.CreateStructGEP(env_type, env_ptr, std::to_underlying(GenEnvField::LOCATION), "slot.get.loc" + uid));
  ctx->Builder.CreateStore(
    val_ptr,
    ctx->Builder.CreateStructGEP(
      env_type, env_ptr, std::to_underlying(GenEnvField::YIELD_SLOT), "slot.get.yield" + uid));
  ctx->Builder.CreateRetVoid();

  // The (only) continuation block: falling out of it with no terminator marks the coroutine EXHAUSTED, matching
  // "GenOnce" (a single yield only).
  const auto cont_bb = llvm::BasicBlock::Create(
    *ctx->Context, "slot.get.cont" + uid, ctx->Builder.GetInsertBlock()->getParent());
  ctx->YieldContinuations.push_back(cont_bb);
  ctx->Builder.SetInsertPoint(cont_bb);
}

auto spp::codegen::func_impls::simple_coro_non_null_fwd(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx)
  -> void {
  const auto coro = proto->To<asts::CoroutinePrototypeAst>();
  SPP_ASSERT(coro != nullptr);
  const auto env_type = coro->LlvmCoroGenEnvType;
  const auto env_ptr = coro->LlvmCoroGenEnv;

  // "self" is "&NonNull[T]" - a borrow, so its frame field holds the *address* of the caller's "NonNull[T]" instance,
  // not the instance itself; that address has to be loaded out before use (see "simple_coro_slot_get"). Unlike
  // "Slot[T]", "NonNull[T]" has no field to "GEP" into - it lowers to a bare "ptr" (see "kNonNullParts" in
  // "llvm_type.cpp") whose own value *is* the address to yield, so a second load reads it directly.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.fwd.self");
  const auto val_ptr = ctx->Builder.CreateLoad(ptr_ty, self_ptr, "non_null.fwd.val_ptr");

  const auto uid = "." + spp::utils::Uid();
  const auto i8_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);

  // Suspend once (mirrors GenExpressionAst): mark YIELDED, record the resume location, and store the address into the
  // yield slot.
  const auto location = ctx->YieldContinuations.Len() + 1;
  ctx->Builder.CreateStore(
    llvm::ConstantInt::get(i8_ty, std::to_underlying(CoroutineState::YIELDED)),
    ctx->Builder.CreateStructGEP(
      env_type, env_ptr, std::to_underlying(GenEnvField::STATE), "non_null.fwd.state" + uid));
  ctx->Builder.CreateStore(
    llvm::ConstantInt::get(i32_ty, location),
    ctx->Builder.CreateStructGEP(
      env_type, env_ptr, std::to_underlying(GenEnvField::LOCATION), "non_null.fwd.loc" + uid));
  ctx->Builder.CreateStore(
    val_ptr,
    ctx->Builder.CreateStructGEP(
      env_type, env_ptr, std::to_underlying(GenEnvField::YIELD_SLOT), "non_null.fwd.yield" + uid));
  ctx->Builder.CreateRetVoid();

  // The (only) continuation block: falling out of it with no terminator marks the coroutine EXHAUSTED, matching
  // "GenOnce" (a single yield only).
  const auto cont_bb = llvm::BasicBlock::Create(
    *ctx->Context, "non_null.fwd.cont" + uid, ctx->Builder.GetInsertBlock()->getParent());
  ctx->YieldContinuations.push_back(cont_bb);
  ctx->Builder.SetInsertPoint(cont_bb);
}

auto spp::codegen::func_impls::simple_vol_val_ptr(
  analyse::scopes::ScopeManager const *sm,
  Shared<analyse::scopes::VariableSymbol> const &self_sym,
  LLvmCtx *ctx)
  -> llvm::Value* {
  // "self" is "&Vol[T]"/"&mut Vol[T]" - a borrow, so its slot holds an address that must be loaded before use as a
  // GEP base (see "simple_coro_slot_get"). "val" is "Vol[T]"'s only field.
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "vol.self");
  const auto vol_ty = llvm::cast<llvm::StructType>(
    GetLlvmType(*sm->CurrentScope->GetTypeSymbol(self_sym->Type.get()), ctx));
  return ctx->Builder.CreateStructGEP(vol_ty, self_ptr, 0, "vol.val_ptr");
}

auto spp::codegen::func_impls::simple_coro_strview_slice(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx)
  -> void {
  const auto coro = proto->To<asts::CoroutinePrototypeAst>();
  SPP_ASSERT(coro != nullptr);
  const auto env_type = coro->LlvmCoroGenEnvType;
  const auto env_ptr = coro->LlvmCoroGenEnv;

  // "self" is "&StrView"/"&mut StrView" - a borrow, so its frame field holds an address that must be loaded before
  // use as a GEP base (see "simple_coro_slot_get").
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "strview.slice.self");
  const auto strview_ty = llvm::cast<llvm::StructType>(
    GetLlvmType(*sm->CurrentScope->GetTypeSymbol(self_sym->Type.get()), ctx));
  const auto usize_ty = strview_ty->getElementType(1);

  // "self.ptr" (field 0): the base address the new view's range is computed from.
  const auto self_data_ptr_field = ctx->Builder.CreateStructGEP(
    strview_ty, self_ptr, 0, "strview.slice.self_ptr_field");
  const auto self_data_ptr = ctx->Builder.CreateLoad(ptr_ty, self_data_ptr_field, "strview.slice.self_data_ptr");

  // "from" and "into" are the two non-"self" parameters; both are plain "USize" values (not references), so their
  // own frame slots already hold the value directly.
  const auto from_param = proto->FnParamGroup->GetAllParams()[0];
  const auto from_sym = sm->CurrentScope->GetVarSymbol(from_param->ExtractName().get());
  const auto from_ptr = from_sym->LlvmInfo->Alloca;
  const auto from_val = ctx->Builder.CreateLoad(usize_ty, from_ptr, "strview.slice.from");

  const auto into_param = proto->FnParamGroup->GetAllParams()[1];
  const auto into_sym = sm->CurrentScope->GetVarSymbol(into_param->ExtractName().get());
  const auto into_ptr = into_sym->LlvmInfo->Alloca;
  const auto into_val = ctx->Builder.CreateLoad(usize_ty, into_ptr, "strview.slice.into");

  // Compute the new view: "ptr = self.ptr + from", "length = into - from".
  const auto i8_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto new_ptr = ctx->Builder.CreateGEP(i8_ty, self_data_ptr, {from_val}, "strview.slice.new_ptr");
  const auto new_length = ctx->Builder.CreateSub(into_val, from_val, "strview.slice.new_length");

  // Overwrite "from"'s and "into"'s (now-dead) frame slots with the new "{ptr, length}", reusing that adjacent
  // 16-byte region as the materialized "StrView" - see the doc comment on the declaration for why this is safe.
  ctx->Builder.CreateStore(new_ptr, from_ptr);
  ctx->Builder.CreateStore(new_length, into_ptr);
  const auto new_view_ptr = from_ptr;

  const auto uid = "." + spp::utils::Uid();
  const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);

  // Suspend once (mirrors GenExpressionAst): mark YIELDED, record the resume location, and yield the address of the
  // newly materialized view.
  const auto location = ctx->YieldContinuations.Len() + 1;
  ctx->Builder.CreateStore(
    llvm::ConstantInt::get(i8_ty, std::to_underlying(CoroutineState::YIELDED)),
    ctx->Builder.CreateStructGEP(
      env_type, env_ptr, std::to_underlying(GenEnvField::STATE), "strview.slice.state" + uid));
  ctx->Builder.CreateStore(
    llvm::ConstantInt::get(i32_ty, location),
    ctx->Builder.CreateStructGEP(
      env_type, env_ptr, std::to_underlying(GenEnvField::LOCATION), "strview.slice.loc" + uid));
  ctx->Builder.CreateStore(
    new_view_ptr,
    ctx->Builder.CreateStructGEP(
      env_type, env_ptr, std::to_underlying(GenEnvField::YIELD_SLOT), "strview.slice.yield" + uid));
  ctx->Builder.CreateRetVoid();

  // The (only) continuation block: falling out of it with no terminator marks the coroutine EXHAUSTED, matching
  // "GenOnce" (a single yield only).
  const auto cont_bb = llvm::BasicBlock::Create(
    *ctx->Context, "strview.slice.cont" + uid, ctx->Builder.GetInsertBlock()->getParent());
  ctx->YieldContinuations.push_back(cont_bb);
  ctx->Builder.SetInsertPoint(cont_bb);
}

auto spp::codegen::func_impls::self_generic_t_llvm_type(
  analyse::scopes::ScopeManager const *sm,
  Shared<analyse::scopes::VariableSymbol> const &self_sym,
  LLvmCtx *ctx)
  -> llvm::Type* {
  const auto self_type = self_sym->Type->WithoutConvention();
  const auto elem_type_ast = self_type->LastTypePart()->GnArgGroup->TypeAt("T")->Val;
  return GetLlvmType(*sm->CurrentScope->GetTypeSymbol(elem_type_ast.get()), ctx);
}

auto spp::codegen::func_impls::simple_coro_view_index(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx)
  -> void {
  const auto coro = proto->To<asts::CoroutinePrototypeAst>();
  SPP_ASSERT(coro != nullptr);
  const auto env_type = coro->LlvmCoroGenEnvType;
  const auto env_ptr = coro->LlvmCoroGenEnv;
  const auto llvm_func = ctx->Builder.GetInsertBlock()->getParent();

  // "self" is "&View[T]"/"&mut View[T]" - a borrow, so its frame field holds an address that must be loaded before
  // use as a GEP base (see "simple_coro_slot_get").
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "view.index.self");
  const auto view_ty = llvm::cast<llvm::StructType>(
    GetLlvmType(*sm->CurrentScope->GetTypeSymbol(self_sym->Type.get()), ctx));
  const auto usize_ty = view_ty->getElementType(1);
  const auto elem_ty = self_generic_t_llvm_type(sm, self_sym, ctx);

  const auto data_ptr_field = ctx->Builder.CreateStructGEP(view_ty, self_ptr, 0, "view.index.data_ptr_field");
  const auto data_ptr = ctx->Builder.CreateLoad(ptr_ty, data_ptr_field, "view.index.data_ptr");
  const auto length_field = ctx->Builder.CreateStructGEP(view_ty, self_ptr, 1, "view.index.length_field");
  const auto length_val = ctx->Builder.CreateLoad(usize_ty, length_field, "view.index.length");

  // "index" is the sole non-"self" parameter.
  const auto index_param = proto->FnParamGroup->GetAllParams()[0];
  const auto index_sym = sm->CurrentScope->GetVarSymbol(index_param->ExtractName().get());
  const auto index_val = ctx->Builder.CreateLoad(usize_ty, index_sym->LlvmInfo->Alloca, "view.index.index");

  // Bounds check: trap immediately if "index >= length" ("will abort if the index is out of bounds").
  const auto in_bounds = ctx->Builder.CreateICmpULT(index_val, length_val, "view.index.in_bounds");
  const auto trap_bb = llvm::BasicBlock::Create(*ctx->Context, "view.index.trap", llvm_func);
  const auto ok_bb = llvm::BasicBlock::Create(*ctx->Context, "view.index.ok", llvm_func);
  ctx->Builder.CreateCondBr(in_bounds, ok_bb, trap_bb);

  ctx->Builder.SetInsertPoint(trap_bb);
  const auto trap_intrinsic = llvm::Intrinsic::getOrInsertDeclaration(ctx->Module.get(), llvm::Intrinsic::trap);
  ctx->Builder.CreateCall(trap_intrinsic, {});
  ctx->Builder.CreateUnreachable();

  ctx->Builder.SetInsertPoint(ok_bb);
  const auto elem_ptr = ctx->Builder.CreateGEP(elem_ty, data_ptr, {index_val}, "view.index.elem_ptr");

  // Suspend once (GenOnce/"Indexed"): yield the element's address.
  const auto uid = "." + spp::utils::Uid();
  const auto i8_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);
  const auto location = ctx->YieldContinuations.Len() + 1;
  ctx->Builder.CreateStore(
    llvm::ConstantInt::get(i8_ty, std::to_underlying(CoroutineState::YIELDED)),
    ctx->Builder.CreateStructGEP(env_type, env_ptr, std::to_underlying(GenEnvField::STATE), "view.index.state" + uid));
  ctx->Builder.CreateStore(
    llvm::ConstantInt::get(i32_ty, location),
    ctx->Builder.CreateStructGEP(env_type, env_ptr, std::to_underlying(GenEnvField::LOCATION), "view.index.loc" + uid));
  ctx->Builder.CreateStore(
    elem_ptr,
    ctx->Builder.CreateStructGEP(
      env_type, env_ptr, std::to_underlying(GenEnvField::YIELD_SLOT), "view.index.yield" + uid));
  ctx->Builder.CreateRetVoid();

  // The (only) continuation block: falling out of it with no terminator marks the coroutine EXHAUSTED, matching
  // "GenOnce" (a single yield only).
  const auto cont_bb = llvm::BasicBlock::Create(*ctx->Context, "view.index.cont" + uid, llvm_func);
  ctx->YieldContinuations.push_back(cont_bb);
  ctx->Builder.SetInsertPoint(cont_bb);
}

auto spp::codegen::func_impls::simple_coro_view_slice(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx)
  -> void {
  const auto coro = proto->To<asts::CoroutinePrototypeAst>();
  SPP_ASSERT(coro != nullptr);
  const auto env_type = coro->LlvmCoroGenEnvType;
  const auto env_ptr = coro->LlvmCoroGenEnv;

  // "self" is "&View[T]"/"&mut View[T]" - a borrow, so its frame field holds an address that must be loaded before
  // use as a GEP base (see "simple_coro_slot_get").
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "view.slice.self");
  const auto view_ty = llvm::cast<llvm::StructType>(
    GetLlvmType(*sm->CurrentScope->GetTypeSymbol(self_sym->Type.get()), ctx));
  const auto usize_ty = view_ty->getElementType(1);
  const auto elem_ty = self_generic_t_llvm_type(sm, self_sym, ctx);

  // "self.ptr" (field 0): the base address the new view's range is computed from.
  const auto self_data_ptr_field = ctx->Builder.CreateStructGEP(view_ty, self_ptr, 0, "view.slice.self_ptr_field");
  const auto self_data_ptr = ctx->Builder.CreateLoad(ptr_ty, self_data_ptr_field, "view.slice.self_data_ptr");

  // "from" and "upto" are the two non-"self" parameters; both are plain "USize" values (not references), so their
  // own frame slots already hold the value directly.
  const auto from_param = proto->FnParamGroup->GetAllParams()[0];
  const auto from_sym = sm->CurrentScope->GetVarSymbol(from_param->ExtractName().get());
  const auto from_ptr = from_sym->LlvmInfo->Alloca;
  const auto from_val = ctx->Builder.CreateLoad(usize_ty, from_ptr, "view.slice.from");

  const auto upto_param = proto->FnParamGroup->GetAllParams()[1];
  const auto upto_sym = sm->CurrentScope->GetVarSymbol(upto_param->ExtractName().get());
  const auto upto_ptr = upto_sym->LlvmInfo->Alloca;
  const auto upto_val = ctx->Builder.CreateLoad(usize_ty, upto_ptr, "view.slice.upto");

  // Compute the new view: "ptr = self.ptr + from" (stepping by "T"'s own size), "length = upto - from".
  const auto new_ptr = ctx->Builder.CreateGEP(elem_ty, self_data_ptr, {from_val}, "view.slice.new_ptr");
  const auto new_length = ctx->Builder.CreateSub(upto_val, from_val, "view.slice.new_length");

  // Overwrite "from"'s and "upto"'s (now-dead) frame slots with the new "{ptr, length}", reusing that adjacent
  // 16-byte region as the materialized "View[T]" - see "simple_coro_strview_slice" for why this is safe.
  ctx->Builder.CreateStore(new_ptr, from_ptr);
  ctx->Builder.CreateStore(new_length, upto_ptr);
  const auto new_view_ptr = from_ptr;

  const auto uid = "." + spp::utils::Uid();
  const auto i8_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);

  const auto location = ctx->YieldContinuations.Len() + 1;
  ctx->Builder.CreateStore(
    llvm::ConstantInt::get(i8_ty, std::to_underlying(CoroutineState::YIELDED)),
    ctx->Builder.CreateStructGEP(env_type, env_ptr, std::to_underlying(GenEnvField::STATE), "view.slice.state" + uid));
  ctx->Builder.CreateStore(
    llvm::ConstantInt::get(i32_ty, location),
    ctx->Builder.CreateStructGEP(env_type, env_ptr, std::to_underlying(GenEnvField::LOCATION), "view.slice.loc" + uid));
  ctx->Builder.CreateStore(
    new_view_ptr,
    ctx->Builder.CreateStructGEP(
      env_type, env_ptr, std::to_underlying(GenEnvField::YIELD_SLOT), "view.slice.yield" + uid));
  ctx->Builder.CreateRetVoid();

  const auto cont_bb = llvm::BasicBlock::Create(
    *ctx->Context, "view.slice.cont" + uid, ctx->Builder.GetInsertBlock()->getParent());
  ctx->YieldContinuations.push_back(cont_bb);
  ctx->Builder.SetInsertPoint(cont_bb);
}

auto spp::codegen::func_impls::simple_coro_view_iter(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  bool reverse)
  -> void {
  const auto coro = proto->To<asts::CoroutinePrototypeAst>();
  SPP_ASSERT(coro != nullptr);
  const auto env_type = coro->LlvmCoroGenEnvType;
  const auto env_ptr = coro->LlvmCoroGenEnv;
  const auto llvm_func = ctx->Builder.GetInsertBlock()->getParent();

  // "self" is "&View[T]"/"&mut View[T]" - a borrow, so its frame field holds an address that must be loaded before
  // use as a GEP base (see "simple_coro_slot_get").
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "view.iter.self");
  const auto view_ty = llvm::cast<llvm::StructType>(
    GetLlvmType(*sm->CurrentScope->GetTypeSymbol(self_sym->Type.get()), ctx));
  const auto usize_ty = view_ty->getElementType(1);
  const auto elem_ty = self_generic_t_llvm_type(sm, self_sym, ctx);

  const auto self_data_ptr_field = ctx->Builder.CreateStructGEP(view_ty, self_ptr, 0, "view.iter.self_ptr_field");
  const auto self_data_ptr = ctx->Builder.CreateLoad(ptr_ty, self_data_ptr_field, "view.iter.self_data_ptr");
  const auto self_length_field = ctx->Builder.CreateStructGEP(view_ty, self_ptr, 1, "view.iter.self_length_field");
  const auto self_length = ctx->Builder.CreateLoad(usize_ty, self_length_field, "view.iter.self_length");

  // The scratch fields ("current element ptr", "remaining count"), reserved right after the normal frame fields (see
  // "NeedsIterScratchFields" in llvm_coros.cpp) - "self" is the only frame variable here, so this is field index
  // "FRAME_START + 1".
  const auto scratch_index = std::to_underlying(GenEnvField::FRAME_START)
    + static_cast<unsigned>(CollectCoroFrameVars(*sm->CurrentScope).Len());
  const auto scratch_ptr_field = ctx->Builder.CreateStructGEP(
    env_type, env_ptr, scratch_index, "view.iter.scratch_ptr_field");
  const auto scratch_count_field = ctx->Builder.CreateStructGEP(
    env_type, env_ptr, scratch_index + 1, "view.iter.scratch_count_field");

  // Initialize the scratch state: forwards starts at the first element; backwards starts at the last.
  const auto init_ptr = reverse
    ? ctx->Builder.CreateGEP(
        elem_ty, self_data_ptr,
        {ctx->Builder.CreateSub(self_length, llvm::ConstantInt::get(usize_ty, 1), "view.iter.last_index")},
        "view.iter.init_ptr")
    : self_data_ptr;
  ctx->Builder.CreateStore(init_ptr, scratch_ptr_field);
  ctx->Builder.CreateStore(self_length, scratch_count_field);

  const auto uid = "." + spp::utils::Uid();
  const auto loop_check_bb = llvm::BasicBlock::Create(*ctx->Context, "view.iter.check" + uid, llvm_func);
  const auto yield_it_bb = llvm::BasicBlock::Create(*ctx->Context, "view.iter.yield" + uid, llvm_func);
  const auto done_bb = llvm::BasicBlock::Create(*ctx->Context, "view.iter.done" + uid, llvm_func);
  ctx->Builder.CreateBr(loop_check_bb);

  // Check whether any elements remain; if not, fall through to "done_bb" with no terminator, so the caller marks the
  // coroutine EXHAUSTED (matching a body that runs off the end after its last "gen").
  ctx->Builder.SetInsertPoint(loop_check_bb);
  const auto remaining = ctx->Builder.CreateLoad(usize_ty, scratch_count_field, "view.iter.remaining");
  const auto is_done = ctx->Builder.CreateICmpEQ(remaining, llvm::ConstantInt::get(usize_ty, 0), "view.iter.is_done");
  ctx->Builder.CreateCondBr(is_done, done_bb, yield_it_bb);

  // Yield the current element, advancing the scratch state for the next resume *before* suspending, so
  // "loop_check_bb" (reached via the continuation below) always sees state that's already ready for the next check.
  ctx->Builder.SetInsertPoint(yield_it_bb);
  const auto current_ptr = ctx->Builder.CreateLoad(ptr_ty, scratch_ptr_field, "view.iter.current_ptr");
  const auto i64_ty = llvm::Type::getInt64Ty(*ctx->Context);
  const auto next_ptr = ctx->Builder.CreateGEP(
    elem_ty, current_ptr, {llvm::ConstantInt::getSigned(i64_ty, reverse ? -1 : 1)}, "view.iter.next_ptr");
  const auto next_remaining = ctx->Builder.CreateSub(
    remaining, llvm::ConstantInt::get(usize_ty, 1), "view.iter.next_remaining");
  ctx->Builder.CreateStore(next_ptr, scratch_ptr_field);
  ctx->Builder.CreateStore(next_remaining, scratch_count_field);

  const auto i8_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);
  const auto location = ctx->YieldContinuations.Len() + 1;
  ctx->Builder.CreateStore(
    llvm::ConstantInt::get(i8_ty, std::to_underlying(CoroutineState::YIELDED)),
    ctx->Builder.CreateStructGEP(env_type, env_ptr, std::to_underlying(GenEnvField::STATE), "view.iter.state" + uid));
  ctx->Builder.CreateStore(
    llvm::ConstantInt::get(i32_ty, location),
    ctx->Builder.CreateStructGEP(env_type, env_ptr, std::to_underlying(GenEnvField::LOCATION), "view.iter.loc" + uid));
  ctx->Builder.CreateStore(
    current_ptr,
    ctx->Builder.CreateStructGEP(
      env_type, env_ptr, std::to_underlying(GenEnvField::YIELD_SLOT), "view.iter.yield" + uid));
  ctx->Builder.CreateRetVoid();

  // The (only) continuation block: every subsequent resume dispatches here, then jumps straight back to the check,
  // since the scratch state has already been advanced.
  const auto cont_bb = llvm::BasicBlock::Create(*ctx->Context, "view.iter.cont" + uid, llvm_func);
  ctx->YieldContinuations.push_back(cont_bb);
  ctx->Builder.SetInsertPoint(cont_bb);
  ctx->Builder.CreateBr(loop_check_bb);

  // "done_bb" becomes the active insert point when this function returns, with no terminator - matching a coroutine
  // body that has simply run off the end after its last "gen".
  ctx->Builder.SetInsertPoint(done_bb);
}

auto spp::codegen::func_impls::raw_buf_data_ptr(
  analyse::scopes::ScopeManager const *sm,
  Shared<analyse::scopes::VariableSymbol> const &self_sym,
  LLvmCtx *ctx)
  -> llvm::Value* {
  // "self" is "&RawBuf[T, A]"/"&mut RawBuf[T, A]" - a borrow, so its slot holds an address that must be loaded before
  // use as a GEP base (see "simple_coro_slot_get"). "ptr" (declaration index 0) is itself a bare "ptr" (it lowers to
  // "NonNull[T]", which is a bare pointer - see "kNonNullParts" in llvm_type.cpp), so one load past the field reaches
  // the buffer's base address directly - the same idiom "simple_coro_view_index" uses for "View[T].ptr".
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "raw_buf.self");
  const auto self_type_sym = sm->CurrentScope->GetTypeSymbol(self_sym->Type.get());
  const auto raw_buf_ty = llvm::cast<llvm::StructType>(GetLlvmType(*self_type_sym, ctx));
  const auto ptr_field_idx = GetPhysicalFieldIndex(*self_type_sym->LlvmInfo, 0);
  const auto ptr_field = ctx->Builder.CreateStructGEP(raw_buf_ty, self_ptr, ptr_field_idx, "raw_buf.ptr_field");
  return ctx->Builder.CreateLoad(ptr_ty, ptr_field, "raw_buf.data_ptr");
}

auto spp::codegen::func_impls::simple_coro_raw_buf_index(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx)
  -> void {
  const auto coro = proto->To<asts::CoroutinePrototypeAst>();
  SPP_ASSERT(coro != nullptr);
  const auto env_type = coro->LlvmCoroGenEnvType;
  const auto env_ptr = coro->LlvmCoroGenEnv;
  const auto llvm_func = ctx->Builder.GetInsertBlock()->getParent();

  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "raw_buf.index.self");
  const auto self_type_sym = sm->CurrentScope->GetTypeSymbol(self_sym->Type.get());
  const auto raw_buf_ty = llvm::cast<llvm::StructType>(GetLlvmType(*self_type_sym, ctx));
  const auto cap_field_idx = GetPhysicalFieldIndex(*self_type_sym->LlvmInfo, 1);
  const auto cap_field = ctx->Builder.CreateStructGEP(raw_buf_ty, self_ptr, cap_field_idx, "raw_buf.index.cap_field");
  const auto usize_ty = raw_buf_ty->getElementType(cap_field_idx);
  const auto cap_val = ctx->Builder.CreateLoad(usize_ty, cap_field, "raw_buf.index.cap");

  const auto data_ptr = raw_buf_data_ptr(sm, self_sym, ctx);
  const auto elem_ty = self_generic_t_llvm_type(sm, self_sym, ctx);

  // "index" is the sole non-"self" parameter.
  const auto index_param = proto->FnParamGroup->GetAllParams()[0];
  const auto index_sym = sm->CurrentScope->GetVarSymbol(index_param->ExtractName().get());
  const auto index_val = ctx->Builder.CreateLoad(usize_ty, index_sym->LlvmInfo->Alloca, "raw_buf.index.index");

  // Bounds check: "index < capacity". Unlike "View::index_ref" (which traps), out of bounds here just yields "None" -
  // see the header doc comment for why the tags below are hardcoded rather than resolved via "GetVariantTag".
  const auto in_bounds = ctx->Builder.CreateICmpULT(index_val, cap_val, "raw_buf.index.in_bounds");
  const auto some_bb = llvm::BasicBlock::Create(*ctx->Context, "raw_buf.index.some", llvm_func);
  const auto none_bb = llvm::BasicBlock::Create(*ctx->Context, "raw_buf.index.none", llvm_func);
  const auto join_bb = llvm::BasicBlock::Create(*ctx->Context, "raw_buf.index.join", llvm_func);
  ctx->Builder.CreateCondBr(in_bounds, some_bb, none_bb);

  const auto yield_ty = env_type->getElementType(std::to_underlying(GenEnvField::YIELD_SLOT));

  ctx->Builder.SetInsertPoint(some_bb);
  const auto elem_addr = ctx->Builder.CreateGEP(elem_ty, data_ptr, index_val, "raw_buf.index.elem_addr");
  const auto some_val = BuildVariant(elem_addr, yield_ty, 0, "raw_buf.index.some", ctx);
  ctx->Builder.CreateBr(join_bb);

  ctx->Builder.SetInsertPoint(none_bb);
  const auto none_val = BuildVariant(nullptr, yield_ty, 1, "raw_buf.index.none", ctx);
  ctx->Builder.CreateBr(join_bb);

  ctx->Builder.SetInsertPoint(join_bb);
  const auto phi = ctx->Builder.CreatePHI(yield_ty, 2, "raw_buf.index.result");
  phi->addIncoming(some_val, some_bb);
  phi->addIncoming(none_val, none_bb);

  const auto uid = "." + spp::utils::Uid();
  const auto i8_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);
  const auto location = ctx->YieldContinuations.Len() + 1;
  ctx->Builder.CreateStore(
    llvm::ConstantInt::get(i8_ty, std::to_underlying(CoroutineState::YIELDED)),
    ctx->Builder.CreateStructGEP(
      env_type, env_ptr, std::to_underlying(GenEnvField::STATE), "raw_buf.index.state" + uid));
  ctx->Builder.CreateStore(
    llvm::ConstantInt::get(i32_ty, location),
    ctx->Builder.CreateStructGEP(
      env_type, env_ptr, std::to_underlying(GenEnvField::LOCATION), "raw_buf.index.loc" + uid));
  ctx->Builder.CreateStore(
    phi,
    ctx->Builder.CreateStructGEP(
      env_type, env_ptr, std::to_underlying(GenEnvField::YIELD_SLOT), "raw_buf.index.yield" + uid));
  ctx->Builder.CreateRetVoid();

  // The (only) continuation block: falling out of it with no terminator marks the coroutine EXHAUSTED, matching
  // "GenOnce" (a single yield only).
  const auto cont_bb = llvm::BasicBlock::Create(*ctx->Context, "raw_buf.index.cont" + uid, llvm_func);
  ctx->YieldContinuations.push_back(cont_bb);
  ctx->Builder.SetInsertPoint(cont_bb);
}

// =========================================================================================================
// Layer 3: BinOp (simple_intrinsic_binop)
// =========================================================================================================

auto spp::codegen::func_impls::std_boolean_and(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, llvm::Type::getInt1Ty(*ctx->Context), BinOp::LogicalAnd);
}

auto spp::codegen::func_impls::std_boolean_ior(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, llvm::Type::getInt1Ty(*ctx->Context), BinOp::LogicalOr);
}

auto spp::codegen::func_impls::std_intrinsics_add(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::Add);
}

auto spp::codegen::func_impls::std_intrinsics_sub(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::Sub);
}

auto spp::codegen::func_impls::std_intrinsics_mul(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::Mul);
}

auto spp::codegen::func_impls::std_intrinsics_sdiv(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::SDiv);
}

auto spp::codegen::func_impls::std_intrinsics_udiv(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::UDiv);
}

auto spp::codegen::func_impls::std_intrinsics_srem(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::SRem);
}

auto spp::codegen::func_impls::std_intrinsics_urem(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::URem);
}

auto spp::codegen::func_impls::std_intrinsics_bit_shl(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::Shl);
}

auto spp::codegen::func_impls::std_intrinsics_bit_shr(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::LShr);
}

auto spp::codegen::func_impls::std_intrinsics_bit_ior(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::Or);
}

auto spp::codegen::func_impls::std_intrinsics_bit_and(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::And);
}

auto spp::codegen::func_impls::std_intrinsics_bit_xor(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::Xor);
}

auto spp::codegen::func_impls::std_intrinsics_eq(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::ICmpEQ);
}

auto spp::codegen::func_impls::std_intrinsics_oeq(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::FCmpOEQ);
}

auto spp::codegen::func_impls::std_intrinsics_ne(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::ICmpNE);
}

auto spp::codegen::func_impls::std_intrinsics_one(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::FCmpONE);
}

auto spp::codegen::func_impls::std_intrinsics_slt(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::ICmpSLT);
}

auto spp::codegen::func_impls::std_intrinsics_ult(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::ICmpULT);
}

auto spp::codegen::func_impls::std_intrinsics_olt(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::FCmpOLT);
}

auto spp::codegen::func_impls::std_intrinsics_sle(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::ICmpSLE);
}

auto spp::codegen::func_impls::std_intrinsics_ule(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::ICmpULE);
}

auto spp::codegen::func_impls::std_intrinsics_ole(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::FCmpOLE);
}

auto spp::codegen::func_impls::std_intrinsics_sgt(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::ICmpSGT);
}

auto spp::codegen::func_impls::std_intrinsics_ugt(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::ICmpUGT);
}

auto spp::codegen::func_impls::std_intrinsics_ogt(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::FCmpOGT);
}

auto spp::codegen::func_impls::std_intrinsics_sge(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::ICmpSGE);
}

auto spp::codegen::func_impls::std_intrinsics_uge(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::ICmpUGE);
}

auto spp::codegen::func_impls::std_intrinsics_oge(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::FCmpOGE);
}

auto spp::codegen::func_impls::std_intrinsics_fadd(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::FAdd);
}

auto spp::codegen::func_impls::std_intrinsics_fsub(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::FSub);
}

auto spp::codegen::func_impls::std_intrinsics_fmul(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::FMul);
}

auto spp::codegen::func_impls::std_intrinsics_fdiv(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::FDiv);
}

auto spp::codegen::func_impls::std_intrinsics_frem(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::FRem);
}

auto spp::codegen::func_impls::std_intrinsics_sadd_wrapping(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::NSWAdd);
}

auto spp::codegen::func_impls::std_intrinsics_uadd_wrapping(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::NUWAdd);
}

auto spp::codegen::func_impls::std_intrinsics_ssub_wrapping(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::NSWSub);
}

auto spp::codegen::func_impls::std_intrinsics_usub_wrapping(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::NUWSub);
}

auto spp::codegen::func_impls::std_intrinsics_smul_wrapping(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::NSWMul);
}

auto spp::codegen::func_impls::std_intrinsics_umul_wrapping(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, ctx, ty, BinOp::NUWMul);
}

// =========================================================================================================
// Layer 3: BinOp (simple_intrinsic_binop_assign)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_add_assign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, ctx, ty, BinOp::Add);
}

auto spp::codegen::func_impls::std_intrinsics_sub_assign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, ctx, ty, BinOp::Sub);
}

auto spp::codegen::func_impls::std_intrinsics_mul_assign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, ctx, ty, BinOp::Mul);
}

auto spp::codegen::func_impls::std_intrinsics_sdiv_assign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, ctx, ty, BinOp::SDiv);
}

auto spp::codegen::func_impls::std_intrinsics_udiv_assign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, ctx, ty, BinOp::UDiv);
}

auto spp::codegen::func_impls::std_intrinsics_srem_assign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, ctx, ty, BinOp::SRem);
}

auto spp::codegen::func_impls::std_intrinsics_urem_assign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, ctx, ty, BinOp::URem);
}

auto spp::codegen::func_impls::std_intrinsics_bit_shl_assign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, ctx, ty, BinOp::Shl);
}

auto spp::codegen::func_impls::std_intrinsics_bit_shr_assign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, ctx, ty, BinOp::LShr);
}

auto spp::codegen::func_impls::std_intrinsics_bit_ior_assign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, ctx, ty, BinOp::Or);
}

auto spp::codegen::func_impls::std_intrinsics_bit_and_assign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, ctx, ty, BinOp::And);
}

auto spp::codegen::func_impls::std_intrinsics_bit_xor_assign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, ctx, ty, BinOp::Xor);
}

auto spp::codegen::func_impls::std_intrinsics_fadd_assign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, ctx, ty, BinOp::FAdd);
}

auto spp::codegen::func_impls::std_intrinsics_fsub_assign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, ctx, ty, BinOp::FSub);
}

auto spp::codegen::func_impls::std_intrinsics_fmul_assign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, ctx, ty, BinOp::FMul);
}

auto spp::codegen::func_impls::std_intrinsics_fdiv_assign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, ctx, ty, BinOp::FDiv);
}

auto spp::codegen::func_impls::std_intrinsics_frem_assign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, ctx, ty, BinOp::FRem);
}

// =========================================================================================================
// Layer 3: UnOp (simple_intrinsic_unop / simple_intrinsic_unop_assign)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_sneg(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_unop(sm, proto, ctx, ty, UnOp::Neg);
}

auto spp::codegen::func_impls::std_intrinsics_fneg(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_unop(sm, proto, ctx, ty, UnOp::FNeg);
}

auto spp::codegen::func_impls::std_intrinsics_bit_not(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_unop(sm, proto, ctx, ty, UnOp::Not);
}

auto spp::codegen::func_impls::std_intrinsics_bit_not_assign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_unop_assign(sm, proto, ctx, ty, UnOp::Not);
}

// =========================================================================================================
// Layer 3: ConvOp (simple_intrinsic_conv)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_sitofp(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, ctx, ty, ConvOp::SIToFP);
}

auto spp::codegen::func_impls::std_intrinsics_uitofp(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, ctx, ty, ConvOp::UIToFP);
}

auto spp::codegen::func_impls::std_intrinsics_fptrunc(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, ctx, ty, ConvOp::FPTrunc);
}

auto spp::codegen::func_impls::std_intrinsics_strunc(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, ctx, ty, ConvOp::Trunc);
}

auto spp::codegen::func_impls::std_intrinsics_utrunc(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, ctx, ty, ConvOp::Trunc);
}

auto spp::codegen::func_impls::std_intrinsics_szext(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, ctx, ty, ConvOp::SExt);
}

auto spp::codegen::func_impls::std_intrinsics_uzext(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, ctx, ty, ConvOp::ZExt);
}

auto spp::codegen::func_impls::std_intrinsics_fpext(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, ctx, ty, ConvOp::FPExt);
}

auto spp::codegen::func_impls::std_intrinsics_bit_cast(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, ctx, ty, ConvOp::BitCast);
}

auto spp::codegen::func_impls::std_intrinsics_fptosi(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, ctx, ty, ConvOp::FPToSI);
}

auto spp::codegen::func_impls::std_intrinsics_fptoui(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, ctx, ty, ConvOp::FPToUI);
}

// =========================================================================================================
// Layer 3: "is this constant" (simple_intrinsic_is_const)
// =========================================================================================================

auto spp::codegen::func_impls::std_num_float_is_zero(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_is_const(sm, proto, ctx, ty, true, 0.0);
}

auto spp::codegen::func_impls::std_num_float_is_one(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_is_const(sm, proto, ctx, ty, true, 1.0);
}

auto spp::codegen::func_impls::std_num_int_is_zero(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_is_const(sm, proto, ctx, ty, false, 0.0);
}

auto spp::codegen::func_impls::std_num_int_is_one(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_is_const(sm, proto, ctx, ty, false, 1.0);
}

// =========================================================================================================
// Layer 3: fixed values (simple_get_value)
// =========================================================================================================

auto spp::codegen::func_impls::std_array_new(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  // The array starts out uninitialized rather than zero-filled; callers that need defined contents go through
  // "new_filled"/"fill", which "mem_set" over this value afterwards.
  const auto val = llvm::UndefValue::get(ty);
  simple_get_value(sm, proto, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_float_neg_one(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantFP::get(ty, -1.0);
  simple_get_value(sm, proto, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_float_zero(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantFP::get(ty, 0.0);
  simple_get_value(sm, proto, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_float_one(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantFP::get(ty, 1.0);
  simple_get_value(sm, proto, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_int_neg_one(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  // All-ones bit pattern is "-1" in two's complement, for any width.
  const auto val = llvm::ConstantInt::get(ty, llvm::APInt::getAllOnes(ty->getIntegerBitWidth()));
  simple_get_value(sm, proto, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_int_zero(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantInt::get(ty, 0);
  simple_get_value(sm, proto, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_int_one(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantInt::get(ty, 1);
  simple_get_value(sm, proto, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_int_two(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantInt::get(ty, 2);
  simple_get_value(sm, proto, ctx, ty, val);
}

auto spp::codegen::func_impls::std_intrinsics_min_val(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  // The lowest representable value for this sized-integer type. LLVM integer types carry no sign, so signedness is
  // read off the resolved "Self" return type's name ("S32" vs "U32") instead of "ty".
  const auto is_signed = proto->ReturnType->LastTypePart()->Name.starts_with("S");
  const auto bit_width = ty->getIntegerBitWidth();
  const auto val = llvm::ConstantInt::get(
    ty, is_signed ? llvm::APInt::getSignedMinValue(bit_width) : llvm::APInt::getMinValue(bit_width));
  simple_get_value(sm, proto, ctx, ty, val);
}

auto spp::codegen::func_impls::std_intrinsics_max_val(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  // The highest representable value for this sized-integer type. See std_intrinsics_min_val for why signedness
  // comes from the return type's name rather than "ty".
  const auto is_signed = proto->ReturnType->LastTypePart()->Name.starts_with("S");
  const auto bit_width = ty->getIntegerBitWidth();
  const auto val = llvm::ConstantInt::get(
    ty, is_signed ? llvm::APInt::getSignedMaxValue(bit_width) : llvm::APInt::getMaxValue(bit_width));
  simple_get_value(sm, proto, ctx, ty, val);
}

auto spp::codegen::func_impls::std_intrinsics_fmin_val(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Most negative finite value representable by this float type.
  const auto val = llvm::ConstantFP::get(*ctx->Context, llvm::APFloat::getLargest(ty->getFltSemantics(), true));
  simple_get_value(sm, proto, ctx, ty, val);
}

auto spp::codegen::func_impls::std_intrinsics_fmax_val(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Largest finite value representable by this float type.
  const auto val = llvm::ConstantFP::get(*ctx->Context, llvm::APFloat::getLargest(ty->getFltSemantics(), false));
  simple_get_value(sm, proto, ctx, ty, val);
}

// =========================================================================================================
// Layer 3: raw LLVM intrinsic calls, "(T, T) -> T" (simple_binary_intrinsic_call)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_smax(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::smax);
}

auto spp::codegen::func_impls::std_intrinsics_umax(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::umax);
}

auto spp::codegen::func_impls::std_intrinsics_smin(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::smin);
}

auto spp::codegen::func_impls::std_intrinsics_umin(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::umin);
}

auto spp::codegen::func_impls::std_intrinsics_fpowi(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::powi);
}

auto spp::codegen::func_impls::std_intrinsics_fpowf(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::pow);
}

auto spp::codegen::func_impls::std_intrinsics_fatan2(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::atan2);
}

auto spp::codegen::func_impls::std_intrinsics_fmax(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::maxnum);
}

auto spp::codegen::func_impls::std_intrinsics_fmin(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::minnum);
}

auto spp::codegen::func_impls::std_intrinsics_fcopysign(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::copysign);
}

auto spp::codegen::func_impls::std_intrinsics_sadd_saturating(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::sadd_sat);
}

auto spp::codegen::func_impls::std_intrinsics_uadd_saturating(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::uadd_sat);
}

auto spp::codegen::func_impls::std_intrinsics_ssub_saturating(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::ssub_sat);
}

auto spp::codegen::func_impls::std_intrinsics_usub_saturating(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::usub_sat);
}

auto spp::codegen::func_impls::std_intrinsics_sshl_saturating(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::sshl_sat);
}

auto spp::codegen::func_impls::std_intrinsics_ushl_saturating(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::ushl_sat);
}

// =========================================================================================================
// Layer 3: raw LLVM intrinsic calls, "(T, T) -> (T, Bool)" (simple_binary_intrinsic_call_overflow)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_sadd_overflow(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call_overflow(sm, proto, ctx, ty, llvm::Intrinsic::sadd_with_overflow);
}

auto spp::codegen::func_impls::std_intrinsics_uadd_overflow(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call_overflow(sm, proto, ctx, ty, llvm::Intrinsic::uadd_with_overflow);
}

auto spp::codegen::func_impls::std_intrinsics_ssub_overflow(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call_overflow(sm, proto, ctx, ty, llvm::Intrinsic::ssub_with_overflow);
}

auto spp::codegen::func_impls::std_intrinsics_usub_overflow(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call_overflow(sm, proto, ctx, ty, llvm::Intrinsic::usub_with_overflow);
}

auto spp::codegen::func_impls::std_intrinsics_smul_overflow(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call_overflow(sm, proto, ctx, ty, llvm::Intrinsic::smul_with_overflow);
}

auto spp::codegen::func_impls::std_intrinsics_umul_overflow(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call_overflow(sm, proto, ctx, ty, llvm::Intrinsic::umul_with_overflow);
}

// =========================================================================================================
// Layer 3: raw LLVM intrinsic calls, "(T) -> T" (simple_unary_intrinsic_call)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_abs(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::abs);
}

auto spp::codegen::func_impls::std_intrinsics_fsqrt(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::sqrt);
}

auto spp::codegen::func_impls::std_intrinsics_fsin(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::sin);
}

auto spp::codegen::func_impls::std_intrinsics_fcos(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::cos);
}

auto spp::codegen::func_impls::std_intrinsics_ftan(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::tan);
}

auto spp::codegen::func_impls::std_intrinsics_fasin(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::asin);
}

auto spp::codegen::func_impls::std_intrinsics_facos(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::acos);
}

auto spp::codegen::func_impls::std_intrinsics_fatan(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::atan);
}

auto spp::codegen::func_impls::std_intrinsics_fsinh(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::sinh);
}

auto spp::codegen::func_impls::std_intrinsics_fcosh(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::cosh);
}

auto spp::codegen::func_impls::std_intrinsics_ftanh(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::tanh);
}

auto spp::codegen::func_impls::std_intrinsics_fexp(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::exp);
}

auto spp::codegen::func_impls::std_intrinsics_fexp2(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::exp2);
}

auto spp::codegen::func_impls::std_intrinsics_fexp10(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::exp10);
}

auto spp::codegen::func_impls::std_intrinsics_flog(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::log);
}

auto spp::codegen::func_impls::std_intrinsics_flog2(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::log2);
}

auto spp::codegen::func_impls::std_intrinsics_flog10(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::log10);
}

auto spp::codegen::func_impls::std_intrinsics_fabs(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::fabs);
}

auto spp::codegen::func_impls::std_intrinsics_ffloor(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::floor);
}

auto spp::codegen::func_impls::std_intrinsics_fceil(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::ceil);
}

auto spp::codegen::func_impls::std_intrinsics_ftrunc(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::trunc);
}

auto spp::codegen::func_impls::std_intrinsics_fround(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::round);
}

auto spp::codegen::func_impls::std_intrinsics_bitreverse(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::bitreverse);
}

auto spp::codegen::func_impls::std_intrinsics_ctlz(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::ctlz);
}

auto spp::codegen::func_impls::std_debug_breakpoint_internal(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, ctx, ty, llvm::Intrinsic::debugtrap);
}

// =========================================================================================================
// Layer 3: three-way integer comparisons. "(this: &T, that: &T) -> S32": "ty" (per the dispatcher) is the return
// type "S32" - "T" is read off "this" instead. "llvm.scmp"/"llvm.ucmp" are overloaded on both the result type and
// the operand type, so both types are passed to "getOrInsertDeclaration".
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_scmp(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto this_param = proto->FnParamGroup->GetAllParams()[0];
  const auto this_sym = sm->CurrentScope->GetVarSymbol(this_param->ExtractName().get());
  const auto operand_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(this_sym->Type.get()), ctx);

  const auto uid = "." + spp::utils::Uid();
  const auto fn = simple_create_fn(sm, proto, ctx, ty, Vec<llvm::Type*>{operand_ty, operand_ty});
  const auto lhs = fn->arg_begin();
  const auto rhs = fn->arg_begin() + 1;
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(
    ctx->Module.get(), llvm::Intrinsic::scmp, {ty, operand_ty});
  const auto result = ctx->Builder.CreateCall(intrinsic_fn, {lhs, rhs}, "intrinsic.result" + uid);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::func_impls::std_intrinsics_ucmp(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto this_param = proto->FnParamGroup->GetAllParams()[0];
  const auto this_sym = sm->CurrentScope->GetVarSymbol(this_param->ExtractName().get());
  const auto operand_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(this_sym->Type.get()), ctx);

  const auto uid = "." + spp::utils::Uid();
  const auto fn = simple_create_fn(sm, proto, ctx, ty, Vec<llvm::Type*>{operand_ty, operand_ty});
  const auto lhs = fn->arg_begin();
  const auto rhs = fn->arg_begin() + 1;
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(
    ctx->Module.get(), llvm::Intrinsic::ucmp, {ty, operand_ty});
  const auto result = ctx->Builder.CreateCall(intrinsic_fn, {lhs, rhs}, "intrinsic.result" + uid);
  ctx->Builder.CreateRet(result);
}

// =========================================================================================================
// Layer 3: bespoke - "fpclass" needs two different argument types plus a Bool return, so it can't go through
// "simple_binary_intrinsic_call" (which assumes both operands and the result share type "ty").
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_fpclass(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  // "(value: T, flag: S32) -> Bool"; "ty" (per the dispatcher) is the return type "Bool" (i1) - "T" is read off the
  // "value" parameter instead.
  const auto value_param = proto->FnParamGroup->GetAllParams()[0];
  const auto value_sym = sm->CurrentScope->GetVarSymbol(value_param->ExtractName().get());
  const auto value_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(value_sym->Type.get()), ctx);

  const auto uid = "." + spp::utils::Uid();
  const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);
  const auto fn = simple_create_fn(sm, proto, ctx, ty, Vec<llvm::Type*>{value_ty, i32_ty});
  const auto value_arg = fn->arg_begin();
  const auto flag_arg = fn->arg_begin() + 1;
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(
    ctx->Module.get(), llvm::Intrinsic::is_fpclass, {value_ty});
  const auto result = ctx->Builder.CreateCall(intrinsic_fn, {value_arg, flag_arg}, "intrinsic.result" + uid);
  ctx->Builder.CreateRet(result);
}

// =========================================================================================================
// Layer 3: bespoke - coroutines / arrays / vectors / slots / futures / memory / atomics.
// =========================================================================================================

auto spp::codegen::func_impls::std_array_iter_mov(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_array_iter_mov(sm, proto, ctx, false);
}

auto spp::codegen::func_impls::std_array_reverse_iter_mov(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_array_iter_mov(sm, proto, ctx, true);
}

auto spp::codegen::func_impls::std_array_fwd_ref(
  analyse::scopes::ScopeManager const *,
  asts::FunctionPrototypeAst const *,
  LLvmCtx *,
  llvm::Type *)
  -> void {
  // We have an [T x n] LLVM array, and are "viewing" into it.
  // Return a { ptr, len } "View" struct
  // TODO
}

auto spp::codegen::func_impls::std_array_fwd_mut(
  analyse::scopes::ScopeManager const *,
  asts::FunctionPrototypeAst const *,
  LLvmCtx *,
  llvm::Type *)
  -> void {
  // We have an [T x n] LLVM array, and are "viewing" into it.
  // Return a { ptr, len } "View" struct
  // TODO
}

auto spp::codegen::func_impls::std_vector_fwd_ref(
  analyse::scopes::ScopeManager const *,
  asts::FunctionPrototypeAst const *,
  LLvmCtx *,
  llvm::Type *)
  -> void {
  // We have an [T x n] LLVM array, and are "viewing" into it.
  // Return a { ptr, len } "View" struct
  // TODO
}

auto spp::codegen::func_impls::std_vector_fwd_mut(
  analyse::scopes::ScopeManager const *,
  asts::FunctionPrototypeAst const *,
  LLvmCtx *,
  llvm::Type *)
  -> void {
  // We have an [T x n] LLVM array, and are "viewing" into it.
  // Return a { ptr, len } "View" struct
  // TODO
}

auto spp::codegen::func_impls::std_generator_send(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *ty)
  -> void {
  // TODO
  (void)sm;
  (void)proto;
  (void)ctx;
  (void)ty;
}

auto spp::codegen::func_impls::std_generator_once_send(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *ty)
  -> void {
  // TODO
  (void)sm;
  (void)proto;
  (void)ctx;
  (void)ty;
}

auto spp::codegen::func_impls::std_future_fut_await(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *)
  -> void {
  // Unlike the other "func_impls::" helpers above, this does not create its own "llvm::Function" via
  // "mangle::mangle_fun_name": "FunctionPrototypeAst::Stage11_CodeGen" has already declared "proto"'s target function
  // (under that mangled name, so call sites can resolve it) and opened its entry block before dispatching here, so
  // this builds directly into that existing function/block instead of leaving it an empty, unterminated declaration
  // while a second, differently-named function goes unreferenced.
  const auto llvm_func = proto->GetLlvmFunc()->Target;

  // "self" (the Fut[T], consumed by value) already has its storage bound to the incoming argument.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto self_ptr = self_sym->LlvmInfo->Alloca;
  const auto llvm_fut_type = llvm::cast<llvm::StructType>(
    GetLlvmType(*sm->CurrentScope->GetTypeSymbol(self_sym->Type.get()), ctx));

  const auto i8_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto loop_bb = llvm::BasicBlock::Create(*ctx->Context, "fut.await.loop", llvm_func);
  const auto yield_bb = llvm::BasicBlock::Create(*ctx->Context, "fut.await.yield", llvm_func);
  const auto done_bb = llvm::BasicBlock::Create(*ctx->Context, "fut.await.done", llvm_func);
  ctx->Builder.CreateBr(loop_bb);

  // Spin-wait on the atomic "state" field (field 0): the spawning "async" closure sets it to COMPLETED with release
  // ordering only after writing the value (field 1), so an acquire load here guarantees the value is visible once
  // COMPLETED is observed.
  ctx->Builder.SetInsertPoint(loop_bb);
  const auto state_ptr = ctx->Builder.CreateStructGEP(llvm_fut_type, self_ptr, 0, "fut.await.state_ptr");
  const auto state_load = ctx->Builder.CreateLoad(i8_ty, state_ptr, "fut.await.state");
  state_load->setAtomic(llvm::AtomicOrdering::Acquire);
  const auto is_done = ctx->Builder.CreateICmpEQ(state_load, llvm::ConstantInt::get(i8_ty, 1), "fut.await.is_done");
  ctx->Builder.CreateCondBr(is_done, done_bb, yield_bb);

  // Not done yet: yield the CPU rather than burning a core, then retry.
  ctx->Builder.SetInsertPoint(yield_bb);
  const auto sched_yield_type = llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx->Context), {}, false);
  const auto sched_yield_func = ctx->Module->getFunction("sppc_sched_yield") != nullptr
    ? ctx->Module->getFunction("sppc_sched_yield")
    : llvm::Function::Create(
      sched_yield_type, llvm::Function::ExternalLinkage, "sppc_sched_yield", ctx->Module.get());
  ctx->Builder.CreateCall(sched_yield_type, sched_yield_func);
  ctx->Builder.CreateBr(loop_bb);

  // Completed: unwrap the "val: Opt[T]" field (field 1). "state == COMPLETED" is only ever set once "val" has been
  // written as "Some", so the tag does not need to be checked here.
  ctx->Builder.SetInsertPoint(done_bb);
  const auto opt_ty = llvm::cast<llvm::StructType>(llvm_fut_type->getElementType(1));
  if (opt_ty->getNumElements() > 1) {
    const auto opt_ptr = ctx->Builder.CreateStructGEP(llvm_fut_type, self_ptr, 1, "fut.await.opt_ptr");
    const auto val_ptr = ctx->Builder.CreateStructGEP(opt_ty, opt_ptr, 0, "fut.await.val_ptr");
    const auto val = ctx->Builder.CreateLoad(opt_ty->getElementType(0), val_ptr, "fut.await.val");
    ctx->Builder.CreateRet(val);
  }
  else {
    // "Opt[Void]": there is no payload sub-object to load.
    ctx->Builder.CreateRetVoid();
  }
}

auto spp::codegen::func_impls::std_future_fut_await_all(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *)
  -> void {
  // Built directly into the already-declared/opened function, same as "std_future_fut_await" above.
  const auto llvm_func = proto->GetLlvmFunc()->Target;

  // "..futures: Fut[T]" is monomorphized to a fixed-size "[Fut[T] x n]" array - the same "Arr[T, n]" lowering
  // "simple_coro_array_iter_mov" uses for "Arr[T, n]::iter_mov" above - so each distinct argument count at a call
  // site is its own instantiation of this function, with "n" baked into the array type.
  const auto variadic_param = proto->FnParamGroup->GetVariadicParams();
  SPP_ASSERT(variadic_param != nullptr);
  const auto futures_sym = sm->CurrentScope->GetVarSymbol(variadic_param->ExtractName().get());
  const auto futures_ptr = futures_sym->LlvmInfo->Alloca;
  const auto arr_ty = llvm::cast<llvm::ArrayType>(
    GetLlvmType(*sm->CurrentScope->GetTypeSymbol(futures_sym->Type.get()), ctx));
  const auto fut_ty = llvm::cast<llvm::StructType>(arr_ty->getElementType());
  const auto n = arr_ty->getNumElements();

  const auto i8_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto i64_ty = llvm::Type::getInt64Ty(*ctx->Context);

  // Spin-wait on each future's atomic "state" field in turn, exactly as "Fut::await" does; the resolved value itself
  // is discarded here, since "await_all" only waits for every future to complete.
  for (auto i = 0uz; i < n; ++i) {
    const auto uid = "." + spp::utils::Uid();
    const auto loop_bb = llvm::BasicBlock::Create(*ctx->Context, "fut.await_all.loop" + uid, llvm_func);
    const auto yield_bb = llvm::BasicBlock::Create(*ctx->Context, "fut.await_all.yield" + uid, llvm_func);
    const auto done_bb = llvm::BasicBlock::Create(*ctx->Context, "fut.await_all.done" + uid, llvm_func);
    ctx->Builder.CreateBr(loop_bb);

    ctx->Builder.SetInsertPoint(loop_bb);
    const auto idx0 = llvm::ConstantInt::get(i64_ty, 0);
    const auto idx1 = llvm::ConstantInt::get(i64_ty, i);
    const auto fut_ptr = ctx->Builder.CreateGEP(arr_ty, futures_ptr, {idx0, idx1}, "fut.await_all.elem" + uid);
    const auto state_ptr = ctx->Builder.CreateStructGEP(fut_ty, fut_ptr, 0, "fut.await_all.state_ptr" + uid);
    const auto state_load = ctx->Builder.CreateLoad(i8_ty, state_ptr, "fut.await_all.state" + uid);
    state_load->setAtomic(llvm::AtomicOrdering::Acquire);
    const auto is_done = ctx->Builder.CreateICmpEQ(
      state_load, llvm::ConstantInt::get(i8_ty, 1), "fut.await_all.is_done" + uid);
    ctx->Builder.CreateCondBr(is_done, done_bb, yield_bb);

    // Not done yet: yield the CPU rather than burning a core, then retry this same future.
    ctx->Builder.SetInsertPoint(yield_bb);
    const auto sched_yield_type = llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx->Context), {}, false);
    const auto sched_yield_func = ctx->Module->getFunction("sppc_sched_yield") != nullptr
      ? ctx->Module->getFunction("sppc_sched_yield")
      : llvm::Function::Create(
        sched_yield_type, llvm::Function::ExternalLinkage, "sppc_sched_yield", ctx->Module.get());
    ctx->Builder.CreateCall(sched_yield_type, sched_yield_func);
    ctx->Builder.CreateBr(loop_bb);

    // Done with this future: move on to the next one (or fall through to the final "ret void" after the loop).
    ctx->Builder.SetInsertPoint(done_bb);
  }

  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_slot_get_ref(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *)
  -> void {
  simple_coro_slot_get(sm, proto, ctx);
}

auto spp::codegen::func_impls::std_slot_get_mut(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *)
  -> void {
  simple_coro_slot_get(sm, proto, ctx);
}

auto spp::codegen::func_impls::std_slot_replace(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *)
  -> void {
  // Built directly into the already-declared function's (already-open) entry block, same as "std_future_fut_await":
  // a plain "fun", not a coroutine, so there's no env/frame indirection - "self" and "val" are ordinary arguments,
  // and no new basic blocks are needed here.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  // "self" is "&Slot[T]" - a borrow, so its slot holds an address that must be loaded before use as a GEP base (see
  // "simple_coro_slot_get").
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "slot.replace.self");
  const auto slot_ty = llvm::cast<llvm::StructType>(
    GetLlvmType(*sm->CurrentScope->GetTypeSymbol(self_sym->Type.get()), ctx));
  const auto val_field_ptr = ctx->Builder.CreateStructGEP(slot_ty, self_ptr, 0, "slot.replace.val_ptr");
  const auto val_ty = slot_ty->getElementType(0);

  // "val" (the new value to store) is the sole non-"self" parameter.
  const auto new_val_param = proto->FnParamGroup->GetAllParams()[0];
  const auto new_val_sym = sm->CurrentScope->GetVarSymbol(new_val_param->ExtractName().get());
  const auto new_val_ptr = new_val_sym->LlvmInfo->Alloca;

  const auto old_val = ctx->Builder.CreateLoad(val_ty, val_field_ptr, "slot.replace.old");
  const auto new_val = ctx->Builder.CreateLoad(val_ty, new_val_ptr, "slot.replace.new");
  ctx->Builder.CreateStore(new_val, val_field_ptr);
  ctx->Builder.CreateRet(old_val);
}

auto spp::codegen::func_impls::std_string_view_slice_ref(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *)
  -> void {
  simple_coro_strview_slice(sm, proto, ctx);
}

auto spp::codegen::func_impls::std_string_view_slice_mut(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *)
  -> void {
  simple_coro_strview_slice(sm, proto, ctx);
}

auto spp::codegen::func_impls::std_view_index_ref(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_view_index(sm, proto, ctx);
}

auto spp::codegen::func_impls::std_view_index_mut(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_view_index(sm, proto, ctx);
}

auto spp::codegen::func_impls::std_view_slice_ref(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_view_slice(sm, proto, ctx);
}

auto spp::codegen::func_impls::std_view_slice_mut(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_view_slice(sm, proto, ctx);
}

auto spp::codegen::func_impls::std_view_iter_ref(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_view_iter(sm, proto, ctx, false);
}

auto spp::codegen::func_impls::std_view_iter_mut(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_view_iter(sm, proto, ctx, false);
}

auto spp::codegen::func_impls::std_view_reverse_iter_ref(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_view_iter(sm, proto, ctx, true);
}

auto spp::codegen::func_impls::std_view_reverse_iter_mut(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_view_iter(sm, proto, ctx, true);
}

auto spp::codegen::func_impls::std_non_null_read(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *,
  LLvmCtx *ctx,
  llvm::Type *ty)
  -> void {
  // "self" is "NonNull[T]" taken by value (not a borrow), so its own frame slot already holds the pointer bits
  // directly - no indirection to load through first, unlike "self_ptr" in "simple_coro_slot_get". One load reads out
  // the address "self" wraps; a second moves the "T" value bit-for-bit out of that address. The caller is
  // responsible for never reading or dropping the source again.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto data_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.read.data_ptr");
  const auto val = ctx->Builder.CreateLoad(ty, data_ptr, "non_null.read.val");
  ctx->Builder.CreateRet(val);
}

auto spp::codegen::func_impls::std_non_null_write(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *)
  -> void {
  // "self" is "&mut NonNull[T]" - a borrow, so a first load reaches the caller's "NonNull[T]" instance, and a second
  // reads the address it wraps (see "std_non_null_read" for why only one load is needed once "self" isn't a borrow).
  // "value" is a plain by-value "T" param, so its own frame slot already holds it directly.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.write.self");
  const auto data_ptr = ctx->Builder.CreateLoad(ptr_ty, self_ptr, "non_null.write.data_ptr");

  const auto value_param = proto->FnParamGroup->GetAllParams()[0];
  const auto value_sym = sm->CurrentScope->GetVarSymbol(value_param->ExtractName().get());
  const auto value_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(value_sym->Type.get()), ctx);
  const auto value_val = ctx->Builder.CreateLoad(value_ty, value_sym->LlvmInfo->Alloca, "non_null.write.value");

  ctx->Builder.CreateStore(value_val, data_ptr);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_non_null_raw(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *,
  LLvmCtx *ctx,
  llvm::Type *ty)
  -> void {
  // "self" is "&NonNull[T]"; the returned "Ptr[T]" rewraps the same address in its "{ addr: USize }" struct (see
  // "Ptr[T]" in pointer.spp) - the inverse of "std_non_null_from_ptr_inner" below.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.raw.self");
  const auto data_ptr = ctx->Builder.CreateLoad(ptr_ty, self_ptr, "non_null.raw.data_ptr");

  const auto ptr_struct_ty = llvm::cast<llvm::StructType>(ty);
  const auto addr_val = ctx->Builder.CreatePtrToInt(data_ptr, ptr_struct_ty->getElementType(0), "non_null.raw.addr");
  const auto undef = llvm::UndefValue::get(ptr_struct_ty);
  const auto result = ctx->Builder.CreateInsertValue(undef, addr_val, {0}, "non_null.raw.result");
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::func_impls::std_non_null_cast(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *,
  LLvmCtx *ctx,
  llvm::Type *)
  -> void {
  // "self" is "NonNull[U8]" taken by value, and the returned "NonNull[U]" is bit-identical - both lower to the same
  // bare "ptr" (see "kNonNullParts" in llvm_type.cpp) - so this just hands the same address back; only the static
  // type changes.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.cast.self");
  ctx->Builder.CreateRet(self_ptr);
}

auto spp::codegen::func_impls::std_non_null_from_ptr_inner(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *ty)
  -> void {
  // "ptr" is a plain by-value "Ptr[T]" ({ addr: USize }), so its own frame slot is already the struct's storage - no
  // borrow indirection to load through. Rebuild the address as a real "ptr" and hand it back as "NonNull[T]" (which
  // itself lowers to a bare "ptr" - see "kNonNullParts" in llvm_type.cpp); the inverse of "std_non_null_raw" above.
  const auto ptr_param = proto->FnParamGroup->GetAllParams()[0];
  const auto ptr_sym = sm->CurrentScope->GetVarSymbol(ptr_param->ExtractName().get());
  const auto ptr_struct_ty = llvm::cast<llvm::StructType>(
    GetLlvmType(*sm->CurrentScope->GetTypeSymbol(ptr_sym->Type.get()), ctx));
  const auto addr_field_ptr = ctx->Builder.CreateStructGEP(
    ptr_struct_ty, ptr_sym->LlvmInfo->Alloca, 0, "non_null.from_ptr.addr_field");
  const auto addr_val = ctx->Builder.CreateLoad(
    ptr_struct_ty->getElementType(0), addr_field_ptr, "non_null.from_ptr.addr");

  const auto data_ptr = ctx->Builder.CreateIntToPtr(addr_val, ty, "non_null.from_ptr.data_ptr");
  ctx->Builder.CreateRet(data_ptr);
}

auto spp::codegen::func_impls::std_non_null_fwd_mut(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_non_null_fwd(sm, proto, ctx);
}

auto spp::codegen::func_impls::std_non_null_fwd_ref(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_non_null_fwd(sm, proto, ctx);
}

auto spp::codegen::func_impls::std_vol_read(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *,
  LLvmCtx *ctx,
  llvm::Type *ty)
  -> void {
  // A plain volatile load out of "Vol[T]"'s own "val" field.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto val_ptr = simple_vol_val_ptr(sm, self_sym, ctx);
  const auto val = ctx->Builder.CreateLoad(ty, val_ptr, true, "vol.read.val");
  ctx->Builder.CreateRet(val);
}

auto spp::codegen::func_impls::std_vol_write(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *)
  -> void {
  // A plain volatile store into "Vol[T]"'s own "val" field. "val" (the incoming parameter) is a plain by-value "T",
  // so its own frame slot already holds it directly - only the store into "Vol[T]"'s field needs to be volatile.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto val_ptr = simple_vol_val_ptr(sm, self_sym, ctx);

  const auto new_val_param = proto->FnParamGroup->GetAllParams()[0];
  const auto new_val_sym = sm->CurrentScope->GetVarSymbol(new_val_param->ExtractName().get());
  const auto val_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(new_val_sym->Type.get()), ctx);
  const auto new_val = ctx->Builder.CreateLoad(val_ty, new_val_sym->LlvmInfo->Alloca, "vol.write.new_val");

  ctx->Builder.CreateStore(new_val, val_ptr, true);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_vol_replace(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *ty)
  -> void {
  // Same shape as "std_slot_replace", except both the read of the old value and the store of the new one must be
  // volatile - "Vol[T]"'s whole point is that neither gets optimized away or reordered relative to the other.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto val_ptr = simple_vol_val_ptr(sm, self_sym, ctx);
  const auto old_val = ctx->Builder.CreateLoad(ty, val_ptr, true, "vol.replace.old_val");

  const auto new_val_param = proto->FnParamGroup->GetAllParams()[0];
  const auto new_val_sym = sm->CurrentScope->GetVarSymbol(new_val_param->ExtractName().get());
  const auto new_val = ctx->Builder.CreateLoad(ty, new_val_sym->LlvmInfo->Alloca, "vol.replace.new_val");

  ctx->Builder.CreateStore(new_val, val_ptr, true);
  ctx->Builder.CreateRet(old_val);
}

auto spp::codegen::func_impls::std_raw_buf_index_ref(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_raw_buf_index(sm, proto, ctx);
}

auto spp::codegen::func_impls::std_raw_buf_index_mut(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_raw_buf_index(sm, proto, ctx);
}

auto spp::codegen::func_impls::std_raw_buf_take_at(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *ty)
  -> void {
  // "Vec::take_at" bounds-checks and confirms the slot is initialized before ever calling this (see
  // "Vec::take_at"/"take_last" in vector.spp), and "RawBuf" has no bitmap or length field of its own to check against
  // (see "raw_buf.spp"'s class doc comment) - so this unconditionally reads a valid, initialized element and always
  // yields "Some(element)".
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto data_ptr = raw_buf_data_ptr(sm, self_sym, ctx);
  const auto elem_ty = self_generic_t_llvm_type(sm, self_sym, ctx);

  const auto index_param = proto->FnParamGroup->GetAllParams()[0];
  const auto index_sym = sm->CurrentScope->GetVarSymbol(index_param->ExtractName().get());
  const auto usize_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(index_sym->Type.get()), ctx);
  const auto index_val = ctx->Builder.CreateLoad(usize_ty, index_sym->LlvmInfo->Alloca, "raw_buf.take_at.index");

  const auto elem_addr = ctx->Builder.CreateGEP(elem_ty, data_ptr, index_val, "raw_buf.take_at.elem_addr");
  const auto elem_val = ctx->Builder.CreateLoad(elem_ty, elem_addr, "raw_buf.take_at.elem");

  // "Opt[T] = Some[T] or None" wraps its value member in "Some[T]" (unlike "Indexed[&T or None]", whose value member
  // is the bare reference), so "elem_val" is packed into a "Some[T]" ({ val: T }) struct before being tagged into the
  // "Opt[T]" variant.
  const auto elem_type_ast = self_sym->Type->WithoutConvention()->LastTypePart()->GnArgGroup->TypeAt("T")->Val;
  const auto some_ty_ast = asts::generate::common_types::SomeType(0, elem_type_ast);
  const auto some_struct_ty = llvm::cast<llvm::StructType>(
    GetLlvmType(*sm->CurrentScope->GetTypeSymbol(some_ty_ast.get()), ctx));
  const auto undef_some = llvm::UndefValue::get(some_struct_ty);
  const auto some_struct_val = ctx->Builder.CreateInsertValue(undef_some, elem_val, {0}, "raw_buf.take_at.some");

  const auto tag = GetVariantTag(*proto->ReturnType, *some_ty_ast, *sm->CurrentScope);
  SPP_ASSERT(tag.has_value());
  const auto result = BuildVariant(some_struct_val, ty, *tag, "raw_buf.take_at.result", ctx);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::func_impls::std_raw_buf_place_at(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *)
  -> void {
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto data_ptr = raw_buf_data_ptr(sm, self_sym, ctx);
  const auto elem_ty = self_generic_t_llvm_type(sm, self_sym, ctx);

  const auto index_param = proto->FnParamGroup->GetAllParams()[0];
  const auto index_sym = sm->CurrentScope->GetVarSymbol(index_param->ExtractName().get());
  const auto usize_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(index_sym->Type.get()), ctx);
  const auto index_val = ctx->Builder.CreateLoad(usize_ty, index_sym->LlvmInfo->Alloca, "raw_buf.place_at.index");

  const auto element_param = proto->FnParamGroup->GetAllParams()[1];
  const auto element_sym = sm->CurrentScope->GetVarSymbol(element_param->ExtractName().get());
  const auto element_val = ctx->Builder.CreateLoad(elem_ty, element_sym->LlvmInfo->Alloca, "raw_buf.place_at.element");

  const auto elem_addr = ctx->Builder.CreateGEP(elem_ty, data_ptr, index_val, "raw_buf.place_at.elem_addr");
  ctx->Builder.CreateStore(element_val, elem_addr);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_raw_buf_shift(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *)
  -> void {
  // memmove semantics: "from"/"upto" may overlap, so a plain memcpy would corrupt the tail of an overlapping shift.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto data_ptr = raw_buf_data_ptr(sm, self_sym, ctx);
  const auto elem_ty = self_generic_t_llvm_type(sm, self_sym, ctx);

  const auto from_param = proto->FnParamGroup->GetAllParams()[0];
  const auto upto_param = proto->FnParamGroup->GetAllParams()[1];
  const auto count_param = proto->FnParamGroup->GetAllParams()[2];
  const auto from_sym = sm->CurrentScope->GetVarSymbol(from_param->ExtractName().get());
  const auto upto_sym = sm->CurrentScope->GetVarSymbol(upto_param->ExtractName().get());
  const auto count_sym = sm->CurrentScope->GetVarSymbol(count_param->ExtractName().get());
  const auto usize_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(from_sym->Type.get()), ctx);
  const auto from_val = ctx->Builder.CreateLoad(usize_ty, from_sym->LlvmInfo->Alloca, "raw_buf.shift.from");
  const auto upto_val = ctx->Builder.CreateLoad(usize_ty, upto_sym->LlvmInfo->Alloca, "raw_buf.shift.upto");
  const auto count_val = ctx->Builder.CreateLoad(usize_ty, count_sym->LlvmInfo->Alloca, "raw_buf.shift.count");

  const auto src_addr = ctx->Builder.CreateGEP(elem_ty, data_ptr, from_val, "raw_buf.shift.src");
  const auto dst_addr = ctx->Builder.CreateGEP(elem_ty, data_ptr, upto_val, "raw_buf.shift.dst");

  auto const &dl = ctx->Module->getDataLayout();
  const auto elem_size = dl.getTypeAllocSize(elem_ty).getFixedValue();
  const auto elem_align = dl.getABITypeAlign(elem_ty);
  const auto byte_count = ctx->Builder.CreateMul(
    count_val, llvm::ConstantInt::get(usize_ty, elem_size), "raw_buf.shift.bytes");

  ctx->Builder.CreateMemMove(dst_addr, elem_align, src_addr, elem_align, byte_count);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_raw_buf_clear_range(
  analyse::scopes::ScopeManager const *,
  asts::FunctionPrototypeAst const *,
  LLvmCtx *ctx,
  llvm::Type *)
  -> void {
  // Todo: no-op stub - see the header doc comment. Destroying "[start, start + count)" in place needs per-element
  // destructor calls, which nothing in the compiler can emit yet.
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_mem_ops_size_of(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *,
  LLvmCtx *ctx,
  llvm::Type *ty)
  -> void {
  // No value parameter carries "T" (contrast "size_of_val"), but generic substitution registers a "TypeSymbol"
  // literally named "T" into this (monomorphized) function's own scope - see the header doc comment.
  const auto t_ast = asts::TypeIdentifierAst::FromString("T");
  const auto t_sym = sm->CurrentScope->GetTypeSymbol(t_ast.get());
  const auto size_val = llvm::ConstantInt::get(ty, SizeOf(*sm, *t_sym->FqName()));
  ctx->Builder.CreateRet(size_val);
}

auto spp::codegen::func_impls::std_mem_ops_align_of(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *,
  LLvmCtx *ctx,
  llvm::Type *ty)
  -> void {
  const auto t_ast = asts::TypeIdentifierAst::FromString("T");
  const auto t_sym = sm->CurrentScope->GetTypeSymbol(t_ast.get());
  const auto align_val = llvm::ConstantInt::get(ty, AlignOf(*sm, *t_sym->FqName()));
  ctx->Builder.CreateRet(align_val);
}

auto spp::codegen::func_impls::std_mem_ops_size_of_val(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *ty)
  -> void {
  // Todo: this needs heap querying
  const auto value_param = proto->FnParamGroup->GetAllParams()[0];
  const auto value_sym = sm->CurrentScope->GetVarSymbol(value_param->ExtractName().get());
  const auto elem_type_ast = value_sym->Type->WithoutConvention();
  const auto size_val = llvm::ConstantInt::get(ty, SizeOf(*sm, *elem_type_ast));
  ctx->Builder.CreateRet(size_val);
}

auto spp::codegen::func_impls::std_mem_ops_align_of_val(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *ty)
  -> void {
  // Todo: this needs heap querying
  const auto value_param = proto->FnParamGroup->GetAllParams()[0];
  const auto value_sym = sm->CurrentScope->GetVarSymbol(value_param->ExtractName().get());
  const auto elem_type_ast = value_sym->Type->WithoutConvention();
  const auto align_val = llvm::ConstantInt::get(ty, AlignOf(*sm, *elem_type_ast));
  ctx->Builder.CreateRet(align_val);
}

auto spp::codegen::func_impls::std_mem_ops_replace(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx,
  llvm::Type *ty)
  -> void {
  // "dest" is "&mut T" - a borrow, so its slot holds an address that must be loaded before use (see
  // "simple_coro_slot_get"). "src" is a plain by-value "T", so its own slot already holds it directly.
  const auto dest_param = proto->FnParamGroup->GetAllParams()[0];
  const auto dest_sym = sm->CurrentScope->GetVarSymbol(dest_param->ExtractName().get());
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto dest_ptr = ctx->Builder.CreateLoad(ptr_ty, dest_sym->LlvmInfo->Alloca, "mem.replace.dest_ptr");

  const auto src_param = proto->FnParamGroup->GetAllParams()[1];
  const auto src_sym = sm->CurrentScope->GetVarSymbol(src_param->ExtractName().get());

  const auto old_val = ctx->Builder.CreateLoad(ty, dest_ptr, "mem.replace.old");
  const auto new_val = ctx->Builder.CreateLoad(ty, src_sym->LlvmInfo->Alloca, "mem.replace.new");
  ctx->Builder.CreateStore(new_val, dest_ptr);
  ctx->Builder.CreateRet(old_val);
}

auto spp::codegen::func_impls::std_mem_ops_drop_in_place(
  analyse::scopes::ScopeManager const *,
  asts::FunctionPrototypeAst const *,
  LLvmCtx *ctx,
  llvm::Type *)
  -> void {
  // Todo: no-op stub - same blocker as "std_raw_buf_clear_range" (see its comment): no destructor-dispatch codegen
  // exists anywhere in the compiler yet for this to call into.
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_threading_atomic_is_lock_free(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx, llvm::Type *)
  -> void {
  // "() -> Bool": no parameters at all (not even "self"), so "T" (Atom[T]'s element type) has to come from "Self"
  // itself, via the same "Self"-type lookup used elsewhere for methods without an actual instance to inspect.
  using asts::generate::common_types_precompiled::SELF_TYPE;
  const auto self_type_sym = sm->CurrentScope->GetTypeSymbol(SELF_TYPE.get(), true);
  const auto atom_ty = llvm::cast<llvm::StructType>(GetLlvmType(*self_type_sym, ctx));
  const auto val_ty = atom_ty->getElementType(0);

  // There's no real "TargetMachine"/"TargetLowering" wired up in this codegen context (the max atomic size a target
  // natively lowers to is a codegen-backend property, and standing one up would mean linking target-specific LLVM
  // libraries), so the DataLayout's largest legal integer width stands in for "the largest atomic size this target
  // supports natively" - correct for this compiler's one hardcoded "x86_64-pc-linux-gnu" triple.
  auto const &dl = ctx->Module->getDataLayout();
  const auto max_atomic_bits = dl.getLargestLegalIntTypeSizeInBits();
  const auto is_lock_free = val_ty->getIntegerBitWidth() <= max_atomic_bits;
  const auto bool_ty = llvm::Type::getInt1Ty(*ctx->Context);
  const auto val = llvm::ConstantInt::getBool(*ctx->Context, is_lock_free);
  simple_get_value(sm, proto, ctx, bool_ty, val);
}

auto spp::codegen::func_impls::std_threading_atomic_fence_inner(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx, llvm::Type *)
  -> void {
  // Create the fence function.
  const auto void_ty = llvm::Type::getVoidTy(*ctx->Context);
  const auto order_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto fn = simple_create_fn(sm, proto, ctx, void_ty, Vec<llvm::Type*>{order_ty});

  // Build the function body.
  const auto order_arg = llvm::cast<llvm::ConstantInt>(fn->arg_begin());
  ctx->Builder.CreateFence(static_cast<llvm::AtomicOrdering>(order_arg->getZExtValue()));
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_threading_atomic_load_inner(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx, llvm::Type *ty) -> void {
  const auto uid = "." + spp::utils::Uid();
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto order_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto fn = simple_create_fn(sm, proto, ctx, ty, Vec<llvm::Type*>{ptr_ty, order_ty});

  const auto ptr_arg = fn->arg_begin();
  const auto order_arg = llvm::cast<llvm::ConstantInt>(fn->arg_begin() + 1);

  const auto load_inst = ctx->Builder.CreateLoad(ty, ptr_arg, "atomic.load" + uid);
  load_inst->setAtomic(static_cast<llvm::AtomicOrdering>(order_arg->getZExtValue()));
  ctx->Builder.CreateRet(load_inst);
}

auto spp::codegen::func_impls::std_threading_atomic_store_inner(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx, llvm::Type *) -> void {
  // "(ptr: &T, val: T, order: U8) -> Void": "ty" (per the dispatcher) is the declared return type "Void" - "T" is
  // read off "val" instead.
  const auto val_param = proto->FnParamGroup->GetAllParams()[1];
  const auto val_sym = sm->CurrentScope->GetVarSymbol(val_param->ExtractName().get());
  const auto val_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(val_sym->Type.get()), ctx);

  const auto void_ty = llvm::Type::getVoidTy(*ctx->Context);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto order_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto fn = simple_create_fn(sm, proto, ctx, void_ty, Vec<llvm::Type*>{ptr_ty, val_ty, order_ty});

  const auto ptr_arg = fn->arg_begin();
  const auto val_arg = fn->arg_begin() + 1;
  const auto order_arg = llvm::cast<llvm::ConstantInt>(fn->arg_begin() + 2);

  const auto store_inst = ctx->Builder.CreateStore(val_arg, ptr_arg);
  store_inst->setAtomic(static_cast<llvm::AtomicOrdering>(order_arg->getZExtValue()));
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_threading_atomic_compex_inner(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx, llvm::Type *ty) -> void {
  // "(ptr: &T, old: T, new: T, success_order: U8, failure_order: U8) -> (T, Bool)": a strong compare-and-swap.
  // "ty" (per the dispatcher) is already the whole return type's own lowering - "(T, Bool)" is a literal struct, so
  // "ty" arrives as exactly "{T, i1}" already; "T" (what "old"/"new"/the pointee actually are) is pulled back out of
  // that struct's first field. "cmpxchg" itself already produces that same "{T, i1}" shape, so its result is
  // returned as-is, with no repacking.
  const auto ret_ty = llvm::cast<llvm::StructType>(ty);
  const auto elem_ty = ret_ty->getElementType(0);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto order_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto fn = simple_create_fn(
    sm, proto, ctx, ret_ty, Vec<llvm::Type*>{ptr_ty, elem_ty, elem_ty, order_ty, order_ty});

  const auto ptr_arg = fn->arg_begin();
  const auto old_arg = fn->arg_begin() + 1;
  const auto new_arg = fn->arg_begin() + 2;
  const auto success_order_arg = llvm::cast<llvm::ConstantInt>(fn->arg_begin() + 3);
  const auto failure_order_arg = llvm::cast<llvm::ConstantInt>(fn->arg_begin() + 4);

  auto const &dl = ctx->Module->getDataLayout();
  const auto cmpxchg_inst = ctx->Builder.CreateAtomicCmpXchg(
    ptr_arg, old_arg, new_arg, dl.getABITypeAlign(elem_ty),
    static_cast<llvm::AtomicOrdering>(success_order_arg->getZExtValue()),
    static_cast<llvm::AtomicOrdering>(failure_order_arg->getZExtValue()));
  ctx->Builder.CreateRet(cmpxchg_inst);
}

auto spp::codegen::func_impls::std_threading_atomic_compex_weak_inner(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx, llvm::Type *ty) -> void {
  // Same as "std_threading_atomic_compex_inner", but "weak": the swap is allowed to fail spuriously (report failure
  // even when the value already matched "old"), which some targets can implement more cheaply in a retry loop.
  const auto ret_ty = llvm::cast<llvm::StructType>(ty);
  const auto elem_ty = ret_ty->getElementType(0);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto order_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto fn = simple_create_fn(
    sm, proto, ctx, ret_ty, Vec<llvm::Type*>{ptr_ty, elem_ty, elem_ty, order_ty, order_ty});

  const auto ptr_arg = fn->arg_begin();
  const auto old_arg = fn->arg_begin() + 1;
  const auto new_arg = fn->arg_begin() + 2;
  const auto success_order_arg = llvm::cast<llvm::ConstantInt>(fn->arg_begin() + 3);
  const auto failure_order_arg = llvm::cast<llvm::ConstantInt>(fn->arg_begin() + 4);

  auto const &dl = ctx->Module->getDataLayout();
  const auto cmpxchg_inst = ctx->Builder.CreateAtomicCmpXchg(
    ptr_arg, old_arg, new_arg, dl.getABITypeAlign(elem_ty),
    static_cast<llvm::AtomicOrdering>(success_order_arg->getZExtValue()),
    static_cast<llvm::AtomicOrdering>(failure_order_arg->getZExtValue()));
  cmpxchg_inst->setWeak(true);
  ctx->Builder.CreateRet(cmpxchg_inst);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_exchange(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, ctx, AtomicRmwOp::Xchg);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_and(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, ctx, AtomicRmwOp::And);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_nand(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, ctx, AtomicRmwOp::Nand);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_or(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, ctx, AtomicRmwOp::Or);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_xor(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, ctx, AtomicRmwOp::Xor);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_not(
  analyse::scopes::ScopeManager const *sm,
  asts::FunctionPrototypeAst const *proto,
  LLvmCtx *ctx, llvm::Type *)
  -> void {
  // "(&self, order: U8) -> Bool", only defined for "Atom[Bool]": flipping a single bit is exactly "self.val ^= true",
  // so this reuses the same "atomicrmw xor" as the other "fetch_*" ops - just with the operand hardcoded to "true"
  // rather than coming from a "val" parameter, since there isn't one here. That shifts "order" down to parameter
  // index 0 (there's no "val" ahead of it), so this doesn't go through "simple_atomic_fetch_rmw" directly.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  // "self" is "&Atom[Bool]" - a borrow, so its frame slot holds an address that must be loaded before use as a GEP
  // base (see "simple_atomic_fetch_rmw").
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "atomic.fetch_not.self");
  const auto atom_ty = llvm::cast<llvm::StructType>(
    GetLlvmType(*sm->CurrentScope->GetTypeSymbol(self_sym->Type.get()), ctx));
  const auto val_field_ptr = ctx->Builder.CreateStructGEP(atom_ty, self_ptr, 0, "atomic.fetch_not.val_ptr");
  const auto val_ty = atom_ty->getElementType(0);
  const auto val_arg = llvm::ConstantInt::getBool(*ctx->Context, true);

  const auto order_param = proto->FnParamGroup->GetAllParams()[0];
  const auto order_sym = sm->CurrentScope->GetVarSymbol(order_param->ExtractName().get());
  const auto order_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto order_arg = llvm::cast<llvm::ConstantInt>(
    ctx->Builder.CreateLoad(order_ty, order_sym->LlvmInfo->Alloca, "atomic.fetch_not.order"));

  auto const &dl = ctx->Module->getDataLayout();
  const auto rmw_inst = ctx->Builder.CreateAtomicRMW(
    apply_atomic_rmw_op(AtomicRmwOp::Xor), val_field_ptr, val_arg, dl.getABITypeAlign(val_ty),
    static_cast<llvm::AtomicOrdering>(order_arg->getZExtValue()));
  ctx->Builder.CreateRet(rmw_inst);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_add(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, ctx, AtomicRmwOp::Add);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_sub(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, ctx, AtomicRmwOp::Sub);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_fadd(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, ctx, AtomicRmwOp::FAdd);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_fsub(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, ctx, AtomicRmwOp::FSub);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_fmax(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, ctx, AtomicRmwOp::FMax);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_fmin(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, ctx, AtomicRmwOp::FMin);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_smax(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, ctx, AtomicRmwOp::Max);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_umax(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, ctx, AtomicRmwOp::UMax);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_smin(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, ctx, AtomicRmwOp::Min);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_umin(
  analyse::scopes::ScopeManager const *sm, asts::FunctionPrototypeAst const *proto, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, ctx, AtomicRmwOp::UMin);
}
