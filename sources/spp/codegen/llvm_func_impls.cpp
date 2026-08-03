module;
#include <spp/macros.hpp>

module spp.codegen.llvm_func_impls;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.coroutine_prototype_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.gen_expression_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

// =========================================================================================================
// Layer 1: function + entry-block creation.
// =========================================================================================================

auto spp::codegen::func_impls::simple_create_fn(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ret_ty, Vec<llvm::Type*> const &param_tys)
  -> llvm::Function* {
  const auto uid = "." + utils::Uid();
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

auto spp::codegen::func_impls::is_cmp_bin_op(
  const BinOp op) -> bool {
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
  LLvmCtx *ctx, const BinOp op, llvm::Value *a, llvm::Value *b)
  -> llvm::Value* {
  const auto name = "result" + utils::Uid();
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
  LLvmCtx *ctx, const UnOp op, llvm::Value *a) -> llvm::Value* {
  const auto name = "result" + utils::Uid();
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
  LLvmCtx *ctx, const ConvOp op, llvm::Value *a, llvm::Type *dest_ty) -> llvm::Value* {
  const auto name = "result" + utils::Uid();
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
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, const BinOp op) -> void {
  // "ty" (per the dispatcher) is always the function's declared RETURN type. For arithmetic ops that's also the
  // operand type ("T, T -> T"). For comparisons the return type is "Bool" (i1), so the *operand* type has to be read
  // off the function's own first parameter instead - "ty" alone can't give us both.
  const auto param0_name = proto->FnParamGroup->GetAllParams()[0]->ExtractName().get();
  const auto param0_type = sm->CurrentScope->GetVarSymbol(param0_name)->Type.get();
  const auto operand_ty = is_cmp_bin_op(op)
    ? GetLlvmType(*sm->CurrentScope->GetTypeSymbol(param0_type), ctx)
    : ty;

  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{operand_ty, operand_ty});
  const auto lhs = fn->arg_begin();
  const auto rhs = fn->arg_begin() + 1;
  ctx->Builder.CreateRet(apply_bin_op(ctx, op, lhs, rhs));
}

auto spp::codegen::func_impls::simple_intrinsic_binop_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *, const BinOp op) -> void {
  // "(this: &mut T, that: T) -> Void": "ty" (per the dispatcher) is the declared return type "Void", not "T" - the
  // operand type is read off "that" (the last parameter) instead, which - unlike "this" - is a plain "T" rather than
  // a reference, so there's no reference-unwrapping ambiguity.
  const auto uid = "." + utils::Uid();
  const auto that_param = proto->FnParamGroup->GetAllParams().Back();
  const auto that_sym = sm->CurrentScope->GetVarSymbol(that_param->ExtractName().get());
  const auto operand_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(that_sym->Type.get()), ctx);

  const auto void_ty = llvm::Type::getVoidTy(*ctx->Context);
  const auto ptr_ty = llvm::cast<llvm::Type>(llvm::PointerType::get(*ctx->Context, 0));
  const auto fn = simple_create_fn(sm, proto, meta, ctx, void_ty, Vec{ptr_ty, operand_ty});

  const auto lhs = fn->arg_begin();
  const auto rhs = fn->arg_begin() + 1;
  const auto loaded_val = ctx->Builder.CreateLoad(operand_ty, lhs, "intrinsic.assign.loaded" + uid);
  const auto result = apply_bin_op(ctx, op, loaded_val, rhs);
  ctx->Builder.CreateStore(result, lhs);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::simple_intrinsic_unop(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, const UnOp op) -> void {
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{ty});
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
  const auto uid = "." + utils::Uid();
  const auto this_param = proto->FnParamGroup->GetAllParams()[0];
  const auto this_sym = sm->CurrentScope->GetVarSymbol(this_param->ExtractName().get());
  const auto operand_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(this_sym->Type.get()), ctx);

  const auto void_ty = llvm::Type::getVoidTy(*ctx->Context);
  const auto ptr_ty = llvm::cast<llvm::Type>(llvm::PointerType::get(*ctx->Context, 0));
  const auto fn = simple_create_fn(sm, proto, meta, ctx, void_ty, Vec{ptr_ty});

  const auto lhs = fn->arg_begin();
  const auto loaded_val = ctx->Builder.CreateLoad(operand_ty, lhs, "intrinsic.assign.loaded" + uid);
  const auto result = apply_un_op(ctx, op, loaded_val);
  ctx->Builder.CreateStore(result, lhs);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::simple_intrinsic_conv(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, const ConvOp op) -> void {
  // "ty" (per the dispatcher) is the function's declared RETURN type - the conversion's destination. The source
  // (operand) type is read off the function's own single parameter instead, since conversions genuinely go from one
  // type to a different one (e.g. "S32 -> F64"), unlike every other builder here where operand type == return type.
  const auto param = proto->FnParamGroup->GetAllParams()[0];
  const auto param_sym = sm->CurrentScope->GetVarSymbol(param->ExtractName().get());
  const auto src_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(param_sym->Type.get()), ctx);
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{src_ty});
  const auto operand = fn->arg_begin();
  ctx->Builder.CreateRet(apply_conv_op(ctx, op, operand, ty));
}

auto spp::codegen::func_impls::simple_intrinsic_is_const(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, const bool is_float, const double value) -> void {
  // "ty" (per the dispatcher) is the declared return type, "Bool" (i1) here - the operand's real type (T) is read
  // off "self" instead ("is_zero(&self) -> Bool" et al).
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto operand_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(self_sym->Type.get()), ctx);
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{operand_ty});
  const auto operand = fn->arg_begin();
  const auto name = "result" + utils::Uid();
  const auto result = is_float
    ? ctx->Builder.CreateFCmpOEQ(operand, llvm::ConstantFP::get(operand_ty, value), name)
    : ctx->Builder.CreateICmpEQ(operand, llvm::ConstantInt::get(operand_ty, static_cast<std::uint64_t>(value)), name);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::func_impls::apply_atomic_rmw_op(
  AtomicRmwOp op) -> llvm::AtomicRMWInst::BinOp {
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
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, const AtomicRmwOp op) -> void {
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
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, const llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void {
  const auto uid = "." + utils::Uid();
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{ty, ty});
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
  const auto uid = "." + utils::Uid();
  const auto ret_ty = llvm::cast<llvm::StructType>(ty);
  const auto elem_ty = ret_ty->getElementType(0);
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ret_ty, Vec{elem_ty, elem_ty});
  const auto lhs = fn->arg_begin();
  const auto rhs = fn->arg_begin() + 1;
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(ctx->Module.get(), intrinsic, {elem_ty});
  const auto result = ctx->Builder.CreateCall(intrinsic_fn, {lhs, rhs}, "intrinsic.result" + uid);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::func_impls::simple_unary_intrinsic_call(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, const llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void {
  const auto uid = "." + utils::Uid();
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{ty});
  const auto operand = fn->arg_begin();
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(ctx->Module.get(), intrinsic, {ty});
  const auto result = ctx->Builder.CreateCall(intrinsic_fn, {operand}, "intrinsic.result" + uid);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::func_impls::simple_get_value(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty, llvm::Value *val) -> void {
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{ty});
  (void)fn;
  ctx->Builder.CreateRet(val);
}

// =========================================================================================================
// Layer 2b: coroutine-specific shared helpers.
// =========================================================================================================

auto spp::codegen::func_impls::simple_coro_iter(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, const bool reverse, const bool borrow) -> void {
  // Implementation strategy for iterating an array - start at the
  // array pointer, and each step, increment the pointer value by
  // the array element size. At each position, load the value out
  // and place it into the yield slot, suspending after (mocks the
  // gen expression).
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "iter.self" + uid);
  const auto arr_ty = llvm::cast<llvm::ArrayType>(
    GetLlvmType(*sm->CurrentScope->GetTypeSymbol(self_sym->Type.get()), ctx));
  const auto elem_ty = arr_ty->getElementType();
  const auto n = arr_ty->getNumElements();
  const auto i = MakeUnique<std::size_t>(not reverse ? n : 0);

  struct CustomExpr : asts::ExpressionAst {
    decltype(i) &I;
    decltype(arr_ty) &ArrTy;
    decltype(elem_ty) &ElemTy;
    decltype(self_ptr) &SelfPtr;
    decltype(borrow) &Borrow;

    CustomExpr(
      decltype(i) &i, decltype(arr_ty) &arr_ty, decltype(elem_ty) &elem_ty, decltype(self_ptr) &self_ptr,
      decltype(borrow) &borrow)
      : I(i), ArrTy(arr_ty), ElemTy(elem_ty), SelfPtr(self_ptr), Borrow(borrow) {}

    SPP_AST_KEY_FUNCTIONS_DEFAULT_IMPL

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, LLvmCtx *ctx) -> llvm::Value* override {
      const auto idx_0 = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*ctx->Context), 0uz);
      const auto idx_i = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*ctx->Context), *I);
      const auto shift = ctx->Builder.CreateGEP(ArrTy, SelfPtr, {idx_0, idx_i});
      const auto value = ctx->Builder.CreateLoad(ElemTy, shift);
      return Borrow ? shift : value;
    }
  };

  const auto mock_gen = MakeUnique<asts::GenExpressionAst>(
    nullptr, nullptr, MakeUnique<CustomExpr>(i, arr_ty, elem_ty, self_ptr, borrow));

  if (not reverse) {
    for (auto j = 0uz; j < n; ++j) {
      *i = j;
      mock_gen->Stage11_CodeGen(sm, meta, ctx);
    }
  }
  else {
    for (auto j = n; j > 0; --j) {
      *i = j;
      mock_gen->Stage11_CodeGen(sm, meta, ctx);
    }
  }
}

auto spp::codegen::func_impls::simple_coro_non_null_fwd(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void {
  // The NonNull[T] type can forward to &T/&mut T - modelled
  // as a pointer to the T type, stored within the NonNull[T].
  // However, because the NonNull type is lowered as a pointer,
  // we can just return "self".
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.fwd.self");

  struct CustomExpr : asts::ExpressionAst {
    decltype(self_ptr) &SelfPtr;

    explicit CustomExpr(
      decltype(self_ptr) &self_ptr)
      : SelfPtr(self_ptr) {}

    SPP_AST_KEY_FUNCTIONS_DEFAULT_IMPL

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, LLvmCtx *ctx) -> llvm::Value* override {
      return SelfPtr;
    }
  };

  const auto mock_gen = MakeUnique<asts::GenExpressionAst>(
    nullptr, nullptr, MakeUnique<CustomExpr>(self_ptr));
  mock_gen->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::codegen::func_impls::simple_coro_view_slice(
  SPP_LLVM_FUNC_INFO,
  LLvmCtx *ctx)
  -> void {
  // To slice a view, we need to GEP in the "from" and
  // "upto" pointers, and return the memory between.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "view.self" + uid);

  // Extract the "from" and "upto" from the symbol's alloca
  // storage in the symbol table.
  const auto from_param = proto->FnParamGroup->Params[0]->ExtractName();
  const auto upto_param = proto->FnParamGroup->Params[1]->ExtractName();

  const auto from_alloca = sm->CurrentScope->GetVarSymbol(from_param.get(), true)->LlvmInfo->Alloca;
  const auto upto_alloca = sm->CurrentScope->GetVarSymbol(upto_param.get(), true)->LlvmInfo->Alloca;

  struct CustomExpr : asts::ExpressionAst {
    SPP_AST_KEY_FUNCTIONS_DEFAULT_IMPL

    decltype(from_alloca) &_FromAlloca;
    decltype(upto_alloca) &_UptoAlloca;
    decltype(self_ptr) &_SelfPtr;
    decltype(uid) &_Uid;

    CustomExpr(
      decltype(from_alloca) &from_alloca, decltype(upto_alloca) &upto_alloca, decltype(self_ptr) &self_ptr,
      decltype(uid) &uid)
      : _FromAlloca(from_alloca), _UptoAlloca(upto_alloca), _SelfPtr(self_ptr), _Uid(uid) {}

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, LLvmCtx *ctx) -> llvm::Value* override {
      // Read the integer values from these alloca storages, and
      // use them for the GEP slicing. The borrow returned points
      // to the same memory as the "self" view, just sliced.
      const auto from_val = ctx->Builder.CreateLoad(
        llvm::Type::getInt64Ty(*ctx->Context), _FromAlloca, "view.slice.from_val" + _Uid);
      const auto upto_val = ctx->Builder.CreateLoad(
        llvm::Type::getInt64Ty(*ctx->Context), _UptoAlloca, "view.slice.upto_val" + _Uid);

      // Perform the GEP slice to index the view whilst pointing
      // to the same storage.
      const auto slice = ctx->Builder.CreateGEP(
        llvm::Type::getInt8Ty(*ctx->Context), _SelfPtr, {from_val, upto_val}, "view.slice.slice" + _Uid);

      return slice;
    }
  };

  const auto mock_gen = MakeUnique<asts::GenExpressionAst>(
    nullptr, nullptr, MakeUnique<CustomExpr>(from_alloca, upto_alloca, self_ptr, uid));
  mock_gen->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::codegen::func_impls::simple_coro_view_index(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx) -> void {
  // To index a view, we need to GEP in the "from" and
  // "upto" pointers, and return the memory between.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "view.self" + uid);

  // Extract the "idx" from the symbol's alloca storage in
  // the symbol table.
  const auto idx_param = proto->FnParamGroup->Params[0]->ExtractName();
  const auto idx_alloca = sm->CurrentScope->GetVarSymbol(idx_param.get(), true)->LlvmInfo->Alloca;

  struct CustomExpr : asts::ExpressionAst {
    SPP_AST_KEY_FUNCTIONS_DEFAULT_IMPL

    decltype(idx_alloca) &_IdxAlloca;
    decltype(self_ptr) &_SelfPtr;
    decltype(uid) &_Uid;

    CustomExpr(
      decltype(idx_alloca) &idx_alloca, decltype(self_ptr) &self_ptr, decltype(uid) &uid):
      _IdxAlloca(idx_alloca), _SelfPtr(self_ptr), _Uid(uid) {}

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, LLvmCtx *ctx) -> llvm::Value* override {
      // Read the integer value from these alloca storages, and
      // use them for the GEP slicing. The borrow returned points
      // to the same memory as the "self" view, just indexed.
      const auto idx_val = ctx->Builder.CreateLoad(
        llvm::Type::getInt64Ty(*ctx->Context), _IdxAlloca, "view.slice.idx_val" + _Uid);

      // Perform the GEP index to index the view whilst pointing
      // to the same storage.
      const auto slice = ctx->Builder.CreateGEP(
        llvm::Type::getInt8Ty(*ctx->Context), _SelfPtr, {idx_val}, "view.slice.slice" + _Uid);

      return slice;
    }
  };

  const auto mock_gen = MakeUnique<asts::GenExpressionAst>(
    nullptr, nullptr, MakeUnique<CustomExpr>(idx_alloca, self_ptr, uid));
  mock_gen->Stage11_CodeGen(sm, meta, ctx);
}

// =========================================================================================================
// Layer 3: BinOp (simple_intrinsic_binop)
// =========================================================================================================

auto spp::codegen::func_impls::std_boolean_and(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, llvm::Type::getInt1Ty(*ctx->Context), BinOp::LogicalAnd);
}

auto spp::codegen::func_impls::std_boolean_ior(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, llvm::Type::getInt1Ty(*ctx->Context), BinOp::LogicalOr);
}

auto spp::codegen::func_impls::std_intrinsics_add(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::Add);
}

auto spp::codegen::func_impls::std_intrinsics_sub(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::Sub);
}

auto spp::codegen::func_impls::std_intrinsics_mul(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::Mul);
}

auto spp::codegen::func_impls::std_intrinsics_sdiv(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::SDiv);
}

auto spp::codegen::func_impls::std_intrinsics_udiv(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::UDiv);
}

auto spp::codegen::func_impls::std_intrinsics_srem(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::SRem);
}

auto spp::codegen::func_impls::std_intrinsics_urem(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::URem);
}

auto spp::codegen::func_impls::std_intrinsics_bit_shl(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::Shl);
}

auto spp::codegen::func_impls::std_intrinsics_bit_shr(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::LShr);
}

auto spp::codegen::func_impls::std_intrinsics_bit_ior(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::Or);
}

auto spp::codegen::func_impls::std_intrinsics_bit_and(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::And);
}

auto spp::codegen::func_impls::std_intrinsics_bit_xor(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::Xor);
}

auto spp::codegen::func_impls::std_intrinsics_eq(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpEQ);
}

auto spp::codegen::func_impls::std_intrinsics_oeq(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FCmpOEQ);
}

auto spp::codegen::func_impls::std_intrinsics_ne(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpNE);
}

auto spp::codegen::func_impls::std_intrinsics_one(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FCmpONE);
}

auto spp::codegen::func_impls::std_intrinsics_slt(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpSLT);
}

auto spp::codegen::func_impls::std_intrinsics_ult(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpULT);
}

auto spp::codegen::func_impls::std_intrinsics_olt(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FCmpOLT);
}

auto spp::codegen::func_impls::std_intrinsics_sle(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpSLE);
}

auto spp::codegen::func_impls::std_intrinsics_ule(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpULE);
}

auto spp::codegen::func_impls::std_intrinsics_ole(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FCmpOLE);
}

auto spp::codegen::func_impls::std_intrinsics_sgt(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpSGT);
}

auto spp::codegen::func_impls::std_intrinsics_ugt(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpUGT);
}

auto spp::codegen::func_impls::std_intrinsics_ogt(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FCmpOGT);
}

auto spp::codegen::func_impls::std_intrinsics_sge(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpSGE);
}

auto spp::codegen::func_impls::std_intrinsics_uge(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpUGE);
}

auto spp::codegen::func_impls::std_intrinsics_oge(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FCmpOGE);
}

auto spp::codegen::func_impls::std_intrinsics_fadd(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FAdd);
}

auto spp::codegen::func_impls::std_intrinsics_fsub(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FSub);
}

auto spp::codegen::func_impls::std_intrinsics_fmul(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FMul);
}

auto spp::codegen::func_impls::std_intrinsics_fdiv(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FDiv);
}

auto spp::codegen::func_impls::std_intrinsics_frem(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FRem);
}

auto spp::codegen::func_impls::std_intrinsics_sadd_wrapping(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::NSWAdd);
}

auto spp::codegen::func_impls::std_intrinsics_uadd_wrapping(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::NUWAdd);
}

auto spp::codegen::func_impls::std_intrinsics_ssub_wrapping(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::NSWSub);
}

auto spp::codegen::func_impls::std_intrinsics_usub_wrapping(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::NUWSub);
}

auto spp::codegen::func_impls::std_intrinsics_smul_wrapping(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::NSWMul);
}

auto spp::codegen::func_impls::std_intrinsics_umul_wrapping(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::NUWMul);
}

// =========================================================================================================
// Layer 3: BinOp (simple_intrinsic_binop_assign)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_add_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::Add);
}

auto spp::codegen::func_impls::std_intrinsics_sub_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::Sub);
}

auto spp::codegen::func_impls::std_intrinsics_mul_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::Mul);
}

auto spp::codegen::func_impls::std_intrinsics_sdiv_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::SDiv);
}

auto spp::codegen::func_impls::std_intrinsics_udiv_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::UDiv);
}

auto spp::codegen::func_impls::std_intrinsics_srem_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::SRem);
}

auto spp::codegen::func_impls::std_intrinsics_urem_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::URem);
}

auto spp::codegen::func_impls::std_intrinsics_bit_shl_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::Shl);
}

auto spp::codegen::func_impls::std_intrinsics_bit_shr_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::LShr);
}

auto spp::codegen::func_impls::std_intrinsics_bit_ior_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::Or);
}

auto spp::codegen::func_impls::std_intrinsics_bit_and_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::And);
}

auto spp::codegen::func_impls::std_intrinsics_bit_xor_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::Xor);
}

auto spp::codegen::func_impls::std_intrinsics_fadd_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::FAdd);
}

auto spp::codegen::func_impls::std_intrinsics_fsub_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::FSub);
}

auto spp::codegen::func_impls::std_intrinsics_fmul_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::FMul);
}

auto spp::codegen::func_impls::std_intrinsics_fdiv_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::FDiv);
}

auto spp::codegen::func_impls::std_intrinsics_frem_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::FRem);
}

// =========================================================================================================
// Layer 3: UnOp (simple_intrinsic_unop / simple_intrinsic_unop_assign)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_sneg(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_unop(sm, proto, meta, ctx, ty, UnOp::Neg);
}

auto spp::codegen::func_impls::std_intrinsics_fneg(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_unop(sm, proto, meta, ctx, ty, UnOp::FNeg);
}

auto spp::codegen::func_impls::std_intrinsics_bit_not(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_unop(sm, proto, meta, ctx, ty, UnOp::Not);
}

auto spp::codegen::func_impls::std_intrinsics_bit_not_assign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_unop_assign(sm, proto, meta, ctx, ty, UnOp::Not);
}

// =========================================================================================================
// Layer 3: ConvOp (simple_intrinsic_conv)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_sitofp(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::SIToFP);
}

auto spp::codegen::func_impls::std_intrinsics_uitofp(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::UIToFP);
}

auto spp::codegen::func_impls::std_intrinsics_fptrunc(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::FPTrunc);
}

auto spp::codegen::func_impls::std_intrinsics_strunc(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::Trunc);
}

auto spp::codegen::func_impls::std_intrinsics_utrunc(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::Trunc);
}

auto spp::codegen::func_impls::std_intrinsics_szext(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::SExt);
}

auto spp::codegen::func_impls::std_intrinsics_uzext(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::ZExt);
}

auto spp::codegen::func_impls::std_intrinsics_fpext(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::FPExt);
}

auto spp::codegen::func_impls::std_intrinsics_bit_cast(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::BitCast);
}

auto spp::codegen::func_impls::std_intrinsics_fptosi(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::FPToSI);
}

auto spp::codegen::func_impls::std_intrinsics_fptoui(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::FPToUI);
}

// =========================================================================================================
// Layer 3: "is this constant" (simple_intrinsic_is_const)
// =========================================================================================================

auto spp::codegen::func_impls::std_num_float_is_zero(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_is_const(sm, proto, meta, ctx, ty, true, 0.0);
}

auto spp::codegen::func_impls::std_num_float_is_one(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_is_const(sm, proto, meta, ctx, ty, true, 1.0);
}

auto spp::codegen::func_impls::std_num_int_is_zero(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_is_const(sm, proto, meta, ctx, ty, false, 0.0);
}

auto spp::codegen::func_impls::std_num_int_is_one(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_is_const(sm, proto, meta, ctx, ty, false, 1.0);
}

// =========================================================================================================
// Layer 3: fixed values (simple_get_value)
// =========================================================================================================

auto spp::codegen::func_impls::std_array_new(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  // The array starts out uninitialized rather than zero-filled; callers that need defined contents go through
  // "new_filled"/"fill", which "mem_set" over this value afterwards.
  const auto val = llvm::UndefValue::get(ty);
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_float_neg_one(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantFP::get(ty, -1.0);
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_float_zero(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantFP::get(ty, 0.0);
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_float_one(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantFP::get(ty, 1.0);
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_int_neg_one(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  // All-ones bit pattern is "-1" in two's complement, for any width.
  const auto val = llvm::ConstantInt::get(ty, llvm::APInt::getAllOnes(ty->getIntegerBitWidth()));
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_int_zero(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantInt::get(ty, 0);
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_int_one(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantInt::get(ty, 1);
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_int_two(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantInt::get(ty, 2);
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_intrinsics_min_val(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  // The lowest representable value for this sized-integer type. LLVM integer types carry no sign, so signedness is
  // read off the resolved "Self" return type's name ("S32" vs "U32") instead of "ty".
  const auto is_signed = proto->ReturnType->LastTypePart()->Name.starts_with("S");
  const auto bit_width = ty->getIntegerBitWidth();
  const auto val = llvm::ConstantInt::get(
    ty, is_signed ? llvm::APInt::getSignedMinValue(bit_width) : llvm::APInt::getMinValue(bit_width));
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_intrinsics_max_val(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  // The highest representable value for this sized-integer type. See std_intrinsics_min_val for why signedness
  // comes from the return type's name rather than "ty".
  const auto is_signed = proto->ReturnType->LastTypePart()->Name.starts_with("S");
  const auto bit_width = ty->getIntegerBitWidth();
  const auto val = llvm::ConstantInt::get(
    ty, is_signed ? llvm::APInt::getSignedMaxValue(bit_width) : llvm::APInt::getMaxValue(bit_width));
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_intrinsics_fmin_val(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Most negative finite value representable by this float type.
  const auto val = llvm::ConstantFP::get(*ctx->Context, llvm::APFloat::getLargest(ty->getFltSemantics(), true));
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_intrinsics_fmax_val(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Largest finite value representable by this float type.
  const auto val = llvm::ConstantFP::get(*ctx->Context, llvm::APFloat::getLargest(ty->getFltSemantics(), false));
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

// =========================================================================================================
// Layer 3: raw LLVM intrinsic calls, "(T, T) -> T" (simple_binary_intrinsic_call)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_smax(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::smax);
}

auto spp::codegen::func_impls::std_intrinsics_umax(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::umax);
}

auto spp::codegen::func_impls::std_intrinsics_smin(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::smin);
}

auto spp::codegen::func_impls::std_intrinsics_umin(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::umin);
}

auto spp::codegen::func_impls::std_intrinsics_fpowi(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::powi);
}

auto spp::codegen::func_impls::std_intrinsics_fpowf(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::pow);
}

auto spp::codegen::func_impls::std_intrinsics_fatan2(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::atan2);
}

auto spp::codegen::func_impls::std_intrinsics_fmax(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::maxnum);
}

auto spp::codegen::func_impls::std_intrinsics_fmin(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::minnum);
}

auto spp::codegen::func_impls::std_intrinsics_fcopysign(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::copysign);
}

auto spp::codegen::func_impls::std_intrinsics_sadd_saturating(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::sadd_sat);
}

auto spp::codegen::func_impls::std_intrinsics_uadd_saturating(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::uadd_sat);
}

auto spp::codegen::func_impls::std_intrinsics_ssub_saturating(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::ssub_sat);
}

auto spp::codegen::func_impls::std_intrinsics_usub_saturating(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::usub_sat);
}

auto spp::codegen::func_impls::std_intrinsics_sshl_saturating(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::sshl_sat);
}

auto spp::codegen::func_impls::std_intrinsics_ushl_saturating(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::ushl_sat);
}

// =========================================================================================================
// Layer 3: raw LLVM intrinsic calls, "(T, T) -> (T, Bool)" (simple_binary_intrinsic_call_overflow)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_sadd_overflow(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call_overflow(sm, proto, meta, ctx, ty, llvm::Intrinsic::sadd_with_overflow);
}

auto spp::codegen::func_impls::std_intrinsics_uadd_overflow(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call_overflow(sm, proto, meta, ctx, ty, llvm::Intrinsic::uadd_with_overflow);
}

auto spp::codegen::func_impls::std_intrinsics_ssub_overflow(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call_overflow(sm, proto, meta, ctx, ty, llvm::Intrinsic::ssub_with_overflow);
}

auto spp::codegen::func_impls::std_intrinsics_usub_overflow(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call_overflow(sm, proto, meta, ctx, ty, llvm::Intrinsic::usub_with_overflow);
}

auto spp::codegen::func_impls::std_intrinsics_smul_overflow(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call_overflow(sm, proto, meta, ctx, ty, llvm::Intrinsic::smul_with_overflow);
}

auto spp::codegen::func_impls::std_intrinsics_umul_overflow(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call_overflow(sm, proto, meta, ctx, ty, llvm::Intrinsic::umul_with_overflow);
}

// =========================================================================================================
// Layer 3: raw LLVM intrinsic calls, "(T) -> T" (simple_unary_intrinsic_call)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_abs(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::abs);
}

auto spp::codegen::func_impls::std_intrinsics_fsqrt(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::sqrt);
}

auto spp::codegen::func_impls::std_intrinsics_fsin(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::sin);
}

auto spp::codegen::func_impls::std_intrinsics_fcos(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::cos);
}

auto spp::codegen::func_impls::std_intrinsics_ftan(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::tan);
}

auto spp::codegen::func_impls::std_intrinsics_fasin(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::asin);
}

auto spp::codegen::func_impls::std_intrinsics_facos(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::acos);
}

auto spp::codegen::func_impls::std_intrinsics_fatan(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::atan);
}

auto spp::codegen::func_impls::std_intrinsics_fsinh(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::sinh);
}

auto spp::codegen::func_impls::std_intrinsics_fcosh(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::cosh);
}

auto spp::codegen::func_impls::std_intrinsics_ftanh(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::tanh);
}

auto spp::codegen::func_impls::std_intrinsics_fexp(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::exp);
}

auto spp::codegen::func_impls::std_intrinsics_fexp2(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::exp2);
}

auto spp::codegen::func_impls::std_intrinsics_fexp10(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::exp10);
}

auto spp::codegen::func_impls::std_intrinsics_flog(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::log);
}

auto spp::codegen::func_impls::std_intrinsics_flog2(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::log2);
}

auto spp::codegen::func_impls::std_intrinsics_flog10(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::log10);
}

auto spp::codegen::func_impls::std_intrinsics_fabs(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::fabs);
}

auto spp::codegen::func_impls::std_intrinsics_ffloor(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::floor);
}

auto spp::codegen::func_impls::std_intrinsics_fceil(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::ceil);
}

auto spp::codegen::func_impls::std_intrinsics_ftrunc(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::trunc);
}

auto spp::codegen::func_impls::std_intrinsics_fround(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::round);
}

auto spp::codegen::func_impls::std_intrinsics_bitreverse(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::bitreverse);
}

auto spp::codegen::func_impls::std_intrinsics_ctlz(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::ctlz);
}

auto spp::codegen::func_impls::std_debug_breakpoint_internal(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::debugtrap);
}

// =========================================================================================================
// Layer 3: three-way integer comparisons. "(this: &T, that: &T) -> S32": "ty" (per the dispatcher) is the return
// type "S32" - "T" is read off "this" instead. "llvm.scmp"/"llvm.ucmp" are overloaded on both the result type and
// the operand type, so both types are passed to "getOrInsertDeclaration".
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_scmp(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto this_param = proto->FnParamGroup->GetAllParams()[0];
  const auto this_sym = sm->CurrentScope->GetVarSymbol(this_param->ExtractName().get());
  const auto operand_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(this_sym->Type.get()), ctx);

  const auto uid = "." + utils::Uid();
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{operand_ty, operand_ty});
  const auto lhs = fn->arg_begin();
  const auto rhs = fn->arg_begin() + 1;
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(
    ctx->Module.get(), llvm::Intrinsic::scmp, {ty, operand_ty});
  const auto result = ctx->Builder.CreateCall(intrinsic_fn, {lhs, rhs}, "intrinsic.result" + uid);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::func_impls::std_intrinsics_ucmp(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto this_param = proto->FnParamGroup->GetAllParams()[0];
  const auto this_sym = sm->CurrentScope->GetVarSymbol(this_param->ExtractName().get());
  const auto operand_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(this_sym->Type.get()), ctx);

  const auto uid = "." + utils::Uid();
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{operand_ty, operand_ty});
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
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty)
  -> void {
  // "(value: T, flag: S32) -> Bool"; "ty" (per the dispatcher) is the return type "Bool" (i1) - "T" is read off the
  // "value" parameter instead.
  const auto value_param = proto->FnParamGroup->GetAllParams()[0];
  const auto value_sym = sm->CurrentScope->GetVarSymbol(value_param->ExtractName().get());
  const auto value_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(value_sym->Type.get()), ctx);

  const auto uid = "." + utils::Uid();
  const auto i32_ty = llvm::cast<llvm::Type>(llvm::Type::getInt32Ty(*ctx->Context));
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{value_ty, i32_ty});
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
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_iter(sm, proto, meta, ctx, false, false);
}

auto spp::codegen::func_impls::std_array_reverse_iter_mov(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_iter(sm, proto, meta, ctx, true, false);
}

auto spp::codegen::func_impls::std_array_fwd_ref(
  SPP_LLVM_FUNC_INFO,
  LLvmCtx *,
  llvm::Type *)
  -> void {
  // We have an [T x n] LLVM array, and are "viewing" into it.
  // Return a { ptr, len } "View" struct
  // TODO
}

auto spp::codegen::func_impls::std_array_fwd_mut(
  SPP_LLVM_FUNC_INFO,
  LLvmCtx *,
  llvm::Type *)
  -> void {
  // We have an [T x n] LLVM array, and are "viewing" into it.
  // Return a { ptr, len } "View" struct
  // TODO
}

auto spp::codegen::func_impls::std_vector_fwd_ref(
  SPP_LLVM_FUNC_INFO,
  LLvmCtx *,
  llvm::Type *)
  -> void {
  // We have an [T x n] LLVM array, and are "viewing" into it.
  // Return a { ptr, len } "View" struct
  // TODO
}

auto spp::codegen::func_impls::std_vector_fwd_mut(
  SPP_LLVM_FUNC_INFO,
  LLvmCtx *,
  llvm::Type *)
  -> void {
  // We have an [T x n] LLVM array, and are "viewing" into it.
  // Return a { ptr, len } "View" struct
  // TODO
}

auto spp::codegen::func_impls::std_generator_send(
  SPP_LLVM_FUNC_INFO,
  LLvmCtx *ctx,
  llvm::Type *)
  -> void {
  // Dummy function for analysis. Still needs terminating. The .res() operator handles the lowering for generators
  // there.
  ctx->Builder.CreateUnreachable();
}

auto spp::codegen::func_impls::std_generator_once_send(
  SPP_LLVM_FUNC_INFO,
  LLvmCtx *ctx,
  llvm::Type *)
  -> void {
  // Dummy function for analysis. Still needs terminating. The .res() operator handles the lowering for generators
  // there.
  ctx->Builder.CreateUnreachable();
}

auto spp::codegen::func_impls::std_slot_get_ref(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(
    ptr_ty, self_sym->LlvmInfo->Alloca, "slot.self" + uid);

  struct CustomExpr : asts::ExpressionAst {
    SPP_AST_KEY_FUNCTIONS_DEFAULT_IMPL

    decltype(self_ptr) &_SelfPtr;

    CustomExpr(
      decltype(self_ptr) &self_ptr) :
      _SelfPtr(self_ptr) {}

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, LLvmCtx *ctx) -> llvm::Value* override {
      return _SelfPtr;
    }
  };

  const auto mock_gen = MakeUnique<asts::GenExpressionAst>(
    nullptr, nullptr, MakeUnique<CustomExpr>(self_ptr));
  mock_gen->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::codegen::func_impls::std_slot_get_mut(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void {
  //
  std_slot_get_ref(sm, proto, meta, ctx, ty);
}

auto spp::codegen::func_impls::std_slot_replace(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "slot.replace.self" + uid);
  const auto slot_ty = llvm::cast<llvm::StructType>(
    GetLlvmType(*sm->CurrentScope->GetTypeSymbol(self_sym->Type.get()), ctx));

  const auto val_field_ptr = ctx->Builder.CreateStructGEP(slot_ty, self_ptr, 0, "slot.replace.val_ptr" + uid);
  const auto val_ty = slot_ty->getElementType(0);

  const auto new_val_param = proto->FnParamGroup->GetAllParams()[0];
  const auto new_val_sym = sm->CurrentScope->GetVarSymbol(new_val_param->ExtractName().get());
  const auto new_val_ptr = new_val_sym->LlvmInfo->Alloca;

  const auto old_val = ctx->Builder.CreateLoad(val_ty, val_field_ptr, "slot.replace.old" + uid);
  const auto new_val = ctx->Builder.CreateLoad(val_ty, new_val_ptr, "slot.replace.new" + uid);
  ctx->Builder.CreateStore(new_val, val_field_ptr);
  ctx->Builder.CreateRet(old_val);
}

auto spp::codegen::func_impls::std_string_view_slice_ref(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  simple_coro_view_slice(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_string_view_slice_mut(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  simple_coro_view_slice(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_view_index_ref(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  simple_coro_view_index(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_view_index_mut(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  simple_coro_view_index(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_view_slice_ref(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  simple_coro_view_slice(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_view_slice_mut(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  simple_coro_view_slice(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_view_iter_ref(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_iter(sm, proto, meta, ctx, false, true);
}

auto spp::codegen::func_impls::std_view_iter_mut(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_iter(sm, proto, meta, ctx, false, true);
}

auto spp::codegen::func_impls::std_view_reverse_iter_ref(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_iter(sm, proto, meta, ctx, true, true);
}

auto spp::codegen::func_impls::std_view_reverse_iter_mut(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_iter(sm, proto, meta, ctx, true, true);
}

auto spp::codegen::func_impls::std_non_null_read(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto data_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.read.data_ptr" + uid);
  const auto val = ctx->Builder.CreateLoad(ty, data_ptr, "non_null.read.val" + uid);
  ctx->Builder.CreateRet(val);
}

auto spp::codegen::func_impls::std_non_null_write(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  //
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
  SPP_LLVM_FUNC_INFO,
  LLvmCtx *ctx,
  llvm::Type *ty)
  -> void {
  //
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
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.cast.self");
  ctx->Builder.CreateRet(self_ptr);
}

auto spp::codegen::func_impls::std_non_null_from_ptr_inner(
  SPP_LLVM_FUNC_INFO,
  LLvmCtx *ctx,
  llvm::Type *ty)
  -> void {
  //
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
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_non_null_fwd(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_non_null_fwd_ref(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_non_null_fwd(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_vol_read(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  using asts::generate::common_types_precompiled::SELF_TYPE;

  //
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(
    ptr_ty, self_sym->LlvmInfo->Alloca, "vol.read.self" + uid);
  const auto self_ty_sym = sm->CurrentScope->GetTypeSymbol(SELF_TYPE.get());
  const auto self_ty = GetLlvmType(*self_ty_sym, ctx);

  // Get the "value" alloca and load the value from it. The
  // field is the first field on the "self" object.
  const auto llvm_val_field = ctx->Builder.CreateStructGEP(
    self_ty, self_ptr, 0, "vol.read.val_field" + uid);

  const auto llvm_val = ctx->Builder.CreateLoad(
    ty, llvm_val_field, true, "vol.read.val" + uid);
  ctx->Builder.CreateRet(llvm_val);
}

auto spp::codegen::func_impls::std_vol_write(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  using asts::generate::common_types_precompiled::SELF_TYPE;

  //
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(
    ptr_ty, self_sym->LlvmInfo->Alloca, "vol.read.self" + uid);
  const auto self_ty_sym = sm->CurrentScope->GetTypeSymbol(SELF_TYPE.get());
  const auto self_ty = GetLlvmType(*self_ty_sym, ctx);

  // Get the llvm representation of the value being written
  // to this volatile value.
  const auto new_param = proto->FnParamGroup->Params[0].get();
  const auto new_alloca = sm->CurrentScope->GetVarSymbol(new_param->ExtractName().get(), true)->LlvmInfo->Alloca;
  const auto new_type = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(new_param->Type.get()), ctx);
  const auto new_val = ctx->Builder.CreateLoad(new_type, new_alloca, "vol.write.new_val" + uid);

  // Get the "value" alloca and store the value into it. The
  // field is the first field on the "self" object.
  const auto llvm_val_field = ctx->Builder.CreateStructGEP(
    self_ty, self_ptr, 0, "vol.read.val_field" + uid);

  ctx->Builder.CreateStore(new_val, llvm_val_field, true);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_vol_replace(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "vol.replace.self" + uid);
  const auto slot_ty = llvm::cast<llvm::StructType>(
    GetLlvmType(*sm->CurrentScope->GetTypeSymbol(self_sym->Type.get()), ctx));

  const auto val_field_ptr = ctx->Builder.CreateStructGEP(slot_ty, self_ptr, 0, "vol.replace.val_ptr" + uid);
  const auto val_ty = slot_ty->getElementType(0);

  const auto new_val_param = proto->FnParamGroup->GetAllParams()[0];
  const auto new_val_sym = sm->CurrentScope->GetVarSymbol(new_val_param->ExtractName().get());
  const auto new_val_ptr = new_val_sym->LlvmInfo->Alloca;

  const auto old_val = ctx->Builder.CreateLoad(val_ty, val_field_ptr, true, "vol.replace.old" + uid);
  const auto new_val = ctx->Builder.CreateLoad(val_ty, new_val_ptr, true, "vol.replace.new" + uid);
  ctx->Builder.CreateStore(new_val, val_field_ptr, true);
  ctx->Builder.CreateRet(old_val);
}

auto spp::codegen::func_impls::std_raw_buf_index_ref(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_view_index(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_raw_buf_index_mut(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_view_index(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_raw_buf_take_at(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  // Bounds-checked "take" which moves an element off of a raw
  // buffer, and either returns None (out of bounds or the slot
  // isn't initialized), or Some(val) containing the taken value.
  /*
  using asts::generate::common_types_precompiled::SELF_VAR;
  using asts::generate::common_types_precompiled::SELF_TYPE;
  const auto uid = utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto self_ty_sym = sm->CurrentScope->GetTypeSymbol(SELF_TYPE.get());
  const auto self_ty = GetLlvmType(*self_ty_sym, ctx);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "vol.replace.self" + uid);

  const auto elem_ty_spp = self_ty_sym->FqName()->LastTypePart()->GnArgGroup->TypeAt("T")->Val->WithoutConvention();
  const auto elem_ty_sym = sm->CurrentScope->GetTypeSymbol(elem_ty_spp.get(), true);
  const auto elem_ty = GetLlvmType(*elem_ty_sym, ctx);

  const auto index_param = proto->FnParamGroup->GetAllParams()[0];
  const auto index_sym = sm->CurrentScope->GetVarSymbol(index_param->ExtractName().get());
  const auto usize_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(index_sym->Type.get()), ctx);
  const auto index_val = ctx->Builder.CreateLoad(usize_ty, index_sym->LlvmInfo->Alloca, "raw_buf.take_at.index");

  const auto capacity_addr = ctx->Builder.CreateStructGEP(
    self_ty, self_ptr, 1, "raw_buf.take_at.capacity_addr");
  const auto capacity = ctx->Builder.CreateLoad(
    usize_ty, capacity_addr, "raw_buf.take_at.capacity");

  // If the index is out of bounds, or the element slot at the given
  // index is not initialised, return None.

  // Bounds branching.
  const auto bounds_check = ctx->Builder.CreateICmpULT(index_val, capacity, "raw_buf.take_at.bounds_check");
  const auto bounds_check_bb = ctx->Builder.GetInsertBlock();
  const auto bounds_check_true_bb = llvm::BasicBlock::Create(
    *ctx->Context, "raw_buf.take_at.bounds_check.true", ctx->Builder.GetInsertBlock()->getParent());
  const auto bounds_check_false_bb = llvm::BasicBlock::Create(
    *ctx->Context, "raw_buf.take_at.bounds_check.false", ctx->Builder.GetInsertBlock()->getParent());
  ctx->Builder.CreateCondBr(bounds_check, bounds_check_true_bb, bounds_check_false_bb);
  ctx->Builder.SetInsertPoint(bounds_check_true_bb);

  const auto elem_addr = ctx->Builder.CreateGEP(elem_ty, self_ptr, index_val, "raw_buf.take_at.elem_addr");
  const auto elem_val = ctx->Builder.CreateLoad(elem_ty, elem_addr, "raw_buf.take_at.elem");
  */

  ctx->Builder.CreateUnreachable();
}

auto spp::codegen::func_impls::std_raw_buf_place_at(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  using asts::generate::common_types_precompiled::SELF_VAR;
  using asts::generate::common_types_precompiled::SELF_TYPE;
  //
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto self_ty_sym = sm->CurrentScope->GetTypeSymbol(SELF_TYPE.get());
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "vol.replace.self" + uid);

  const auto elem_ty_spp = self_ty_sym->FqName()->LastTypePart()->GnArgGroup->TypeAt("T")->Val->WithoutConvention();
  const auto elem_ty_sym = sm->CurrentScope->GetTypeSymbol(elem_ty_spp.get(), true);
  const auto elem_ty = GetLlvmType(*elem_ty_sym, ctx);

  const auto index_param = proto->FnParamGroup->GetAllParams()[0];
  const auto index_sym = sm->CurrentScope->GetVarSymbol(index_param->ExtractName().get());
  const auto usize_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(index_sym->Type.get()), ctx);
  const auto index_val = ctx->Builder.CreateLoad(usize_ty, index_sym->LlvmInfo->Alloca, "raw_buf.place_at.index");

  const auto element_param = proto->FnParamGroup->GetAllParams()[1];
  const auto element_sym = sm->CurrentScope->GetVarSymbol(element_param->ExtractName().get());
  const auto element_val = ctx->Builder.CreateLoad(elem_ty, element_sym->LlvmInfo->Alloca, "raw_buf.place_at.element");

  const auto elem_addr = ctx->Builder.CreateGEP(elem_ty, self_ptr, index_val, "raw_buf.place_at.elem_addr");
  ctx->Builder.CreateStore(element_val, elem_addr);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_raw_buf_shift(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  using asts::generate::common_types_precompiled::SELF_TYPE;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto self_ty_sym = sm->CurrentScope->GetTypeSymbol(SELF_TYPE.get());
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "vol.replace.self" + uid);

  const auto elem_ty_spp = self_ty_sym->FqName()->LastTypePart()->GnArgGroup->TypeAt("T")->Val->WithoutConvention();
  const auto elem_ty_sym = sm->CurrentScope->GetTypeSymbol(elem_ty_spp.get(), true);
  const auto elem_ty = GetLlvmType(*elem_ty_sym, ctx);

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

  const auto src_addr = ctx->Builder.CreateGEP(elem_ty, self_ptr, from_val, "raw_buf.shift.src");
  const auto dst_addr = ctx->Builder.CreateGEP(elem_ty, self_ptr, upto_val, "raw_buf.shift.dst");

  auto const &dl = ctx->Module->getDataLayout();
  const auto elem_size = dl.getTypeAllocSize(elem_ty).getFixedValue();
  const auto elem_align = dl.getABITypeAlign(elem_ty);
  const auto byte_count = ctx->Builder.CreateMul(
    count_val, llvm::ConstantInt::get(usize_ty, elem_size), "raw_buf.shift.bytes");

  ctx->Builder.CreateMemMove(dst_addr, elem_align, src_addr, elem_align, byte_count);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_raw_buf_clear_range(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  // Todo: no-op stub - see the header doc comment. Destroying "[start, start + count)" in place needs per-element
  // destructor calls, which nothing in the compiler can emit yet.
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_mem_ops_size_of(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void {
  //
  const auto t_ast = asts::TypeIdentifierAst::FromString("T");
  const auto t_sym = sm->CurrentScope->GetTypeSymbol(t_ast.get());
  const auto size_val = llvm::ConstantInt::get(ty, SizeOf(*sm, *t_sym->FqName()));
  ctx->Builder.CreateRet(size_val);
}

auto spp::codegen::func_impls::std_mem_ops_align_of(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void {
  //
  const auto t_ast = asts::TypeIdentifierAst::FromString("T");
  const auto t_sym = sm->CurrentScope->GetTypeSymbol(t_ast.get());
  const auto align_val = llvm::ConstantInt::get(ty, AlignOf(*sm, *t_sym->FqName()));
  ctx->Builder.CreateRet(align_val);
}

auto spp::codegen::func_impls::std_mem_ops_size_of_val(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void {
  // Todo: this needs heap querying
  const auto value_param = proto->FnParamGroup->GetAllParams()[0];
  const auto value_sym = sm->CurrentScope->GetVarSymbol(value_param->ExtractName().get());
  const auto elem_type_ast = value_sym->Type->WithoutConvention();
  const auto size_val = llvm::ConstantInt::get(ty, SizeOf(*sm, *elem_type_ast));
  ctx->Builder.CreateRet(size_val);
}

auto spp::codegen::func_impls::std_mem_ops_align_of_val(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void {
  // Todo: this needs heap querying
  const auto value_param = proto->FnParamGroup->GetAllParams()[0];
  const auto value_sym = sm->CurrentScope->GetVarSymbol(value_param->ExtractName().get());
  const auto elem_type_ast = value_sym->Type->WithoutConvention();
  const auto align_val = llvm::ConstantInt::get(ty, AlignOf(*sm, *elem_type_ast));
  ctx->Builder.CreateRet(align_val);
}

auto spp::codegen::func_impls::std_mem_ops_replace(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void {
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
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  // Todo: no-op stub - same blocker as "std_raw_buf_clear_range" (see its comment): no destructor-dispatch codegen
  //  exists anywhere in the compiler yet for this to call into.
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_threading_atomic_is_lock_free(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_TYPE;
  const auto self_type_sym = sm->CurrentScope->GetTypeSymbol(SELF_TYPE.get(), true);
  const auto atom_ty = llvm::cast<llvm::StructType>(GetLlvmType(*self_type_sym, ctx));
  const auto val_ty = atom_ty->getElementType(0);

  //
  auto const &dl = ctx->Module->getDataLayout();
  const auto max_atomic_bits = dl.getLargestLegalIntTypeSizeInBits();
  const auto is_lock_free = val_ty->getIntegerBitWidth() <= max_atomic_bits;
  const auto bool_ty = llvm::Type::getInt1Ty(*ctx->Context);
  const auto val = llvm::ConstantInt::getBool(*ctx->Context, is_lock_free);
  simple_get_value(sm, proto, meta, ctx, bool_ty, val);
}

auto spp::codegen::func_impls::std_threading_atomic_fence_inner(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  // Create the fence function.
  const auto void_ty = llvm::Type::getVoidTy(*ctx->Context);
  const auto order_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto fn = simple_create_fn(sm, proto, meta, ctx, void_ty, Vec<llvm::Type*>{order_ty});

  // Build the function body.
  const auto order_arg = llvm::cast<llvm::ConstantInt>(fn->arg_begin());
  ctx->Builder.CreateFence(static_cast<llvm::AtomicOrdering>(order_arg->getZExtValue()));
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_threading_atomic_load_inner(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void {
  //
  const auto uid = "." + utils::Uid();
  const auto ptr_ty = llvm::cast<llvm::Type>(llvm::PointerType::get(*ctx->Context, 0));
  const auto order_ty = llvm::cast<llvm::Type>(llvm::Type::getInt8Ty(*ctx->Context));
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{ptr_ty, order_ty});

  const auto ptr_arg = fn->arg_begin();
  const auto order_arg = llvm::cast<llvm::ConstantInt>(fn->arg_begin() + 1);

  const auto load_inst = ctx->Builder.CreateLoad(ty, ptr_arg, "atomic.load" + uid);
  load_inst->setAtomic(static_cast<llvm::AtomicOrdering>(order_arg->getZExtValue()));
  ctx->Builder.CreateRet(load_inst);
}

auto spp::codegen::func_impls::std_threading_atomic_store_inner(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  //
  const auto val_param = proto->FnParamGroup->GetAllParams()[1];
  const auto val_sym = sm->CurrentScope->GetVarSymbol(val_param->ExtractName().get());
  const auto val_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(val_sym->Type.get()), ctx);

  const auto void_ty = llvm::Type::getVoidTy(*ctx->Context);
  const auto ptr_ty = llvm::cast<llvm::Type>(llvm::PointerType::get(*ctx->Context, 0));
  const auto order_ty = llvm::cast<llvm::Type>(llvm::Type::getInt8Ty(*ctx->Context));
  const auto fn = simple_create_fn(sm, proto, meta, ctx, void_ty, Vec{ptr_ty, val_ty, order_ty});

  const auto ptr_arg = fn->arg_begin();
  const auto val_arg = fn->arg_begin() + 1;
  const auto order_arg = llvm::cast<llvm::ConstantInt>(fn->arg_begin() + 2);

  const auto store_inst = ctx->Builder.CreateStore(val_arg, ptr_arg);
  store_inst->setAtomic(static_cast<llvm::AtomicOrdering>(order_arg->getZExtValue()));
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_threading_atomic_compex_inner(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void {
  //
  const auto ret_ty = llvm::cast<llvm::StructType>(ty);
  const auto elem_ty = ret_ty->getElementType(0);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto order_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto fn = simple_create_fn(
    sm, proto, meta, ctx, ret_ty, Vec<llvm::Type*>{ptr_ty, elem_ty, elem_ty, order_ty, order_ty});

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
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *ty) -> void {
  //
  const auto ret_ty = llvm::cast<llvm::StructType>(ty);
  const auto elem_ty = ret_ty->getElementType(0);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto order_ty = llvm::Type::getInt8Ty(*ctx->Context);
  const auto fn = simple_create_fn(
    sm, proto, meta, ctx, ret_ty, Vec<llvm::Type*>{ptr_ty, elem_ty, elem_ty, order_ty, order_ty});

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
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::Xchg);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_and(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::And);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_nand(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::Nand);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_or(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::Or);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_xor(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::Xor);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_not(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
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
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::Add);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_sub(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::Sub);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_fadd(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::FAdd);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_fsub(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::FSub);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_fmax(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::FMax);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_fmin(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::FMin);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_smax(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::Max);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_umax(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::UMax);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_smin(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::Min);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_umin(
  SPP_LLVM_FUNC_INFO, LLvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::UMin);
}

#pragma GCC diagnostic pop
