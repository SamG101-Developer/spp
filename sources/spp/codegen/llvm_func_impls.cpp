module;
#include <spp/macros.hpp>

module spp.codegen.llvm_func_impls;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.drop_utils;
import spp.analyse.utils.type_members;
import spp.asts.boolean_literal_ast;
import spp.asts.coroutine_prototype_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.gen_expression_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_drop;
import spp.codegen.llvm_func;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_mangle;
import spp.codegen.llvm_size;
import spp.codegen.llvm_type;
import spp.utils.ptr;
import spp.utils.types;
import spp.utils.uid;
import llvm;
import std;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

// =========================================================================================================
// Layer 1: function + entry-block creation.
// =========================================================================================================

namespace {
  /**
   * Emit a message to stderr and abort, terminating the current block.
   *
   * @n
   * Used wherever a runtime contract is broken and the failure has something worth saying. The alternative - a bare
   * @c llvm.trap - lowers to @c ud2 and surfaces as "Illegal instruction" with no index, no length, no location and no
   * name: a good deal less than the failure actually knows. @c dprintf is used rather than @c fprintf because it takes
   * a descriptor directly, so no @c FILE* has to be reached for from ir.
   *
   * @param ctx The llvm context to emit into, positioned at the block that fails.
   * @param fmt The message, as a printf format; a newline is appended.
   * @param args The values for @p fmt 's conversions, in order.
   */
  auto EmitRuntimeAbort(
    spp::codegen::LlvmCtx *const ctx,
    spp::Str const &fmt,
    spp::Vec<llvm::Value*> const &args = {})
    -> void {
    auto *const mod = ctx->Builder.GetInsertBlock()->getParent()->getParent();
    const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);
    const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);

    const auto dprintf_fn = mod->getOrInsertFunction(
      "dprintf", llvm::FunctionType::get(i32_ty, {i32_ty, ptr_ty}, true));

    auto call_args = std::vector<llvm::Value*>{ // Todo: Vec
      llvm::ConstantInt::get(i32_ty, 2),
      ctx->Builder.CreateGlobalString(spp::Str(fmt) + "\n")};
    call_args.insert(call_args.end(), args.begin(), args.end());
    ctx->Builder.CreateCall(dprintf_fn, call_args);

    ctx->Builder.CreateCall(
      mod->getOrInsertFunction("sppc_abort", llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx->Context), {}, false)),
      {});
    ctx->Builder.CreateUnreachable();
  }

  /**
   * The type a @c "Self" symbol stands for.
   *
   * @n
   * @c TypeSymbol::FqName gives @c "Self" back as written, by design: two prototype-less @c "Self" symbols matching
   * each other is what makes a method written in terms of it recognisable as overriding an abstract one. A reader that
   * needs the generic arguments of the type being stood for therefore has to go through the scope the symbol links to,
   * which is the instantiation itself.
   *
   * @param self_ty_sym The symbol to resolve, whether or not it is a @c "Self" one.
   * @return The linked type's fully qualified name, or the symbol's own when it links to nothing.
   */
  auto SelfTypeName(
    spp::analyse::scopes::TypeSymbol const &self_ty_sym)
    -> spp::Shared<spp::asts::TypeAst> {
    return self_ty_sym.LinkedScope != nullptr and self_ty_sym.LinkedScope->TySym != nullptr
      ? self_ty_sym.LinkedScope->TySym->FqName()
      : self_ty_sym.FqName();
  }

  /**
   * Emit a shift whose result is defined for every distance, including one at or past the operand's width.
   *
   * @n
   * A bare @c shl or @c lshr is poison once the distance reaches the operand's bit width, and the hardware does not
   * agree with the language about what that means: x86 masks a variable shift count to the low five or six bits, so
   * @c "x >> 32" on a 32-bit value is assembled as a shift by zero and hands back @p a unchanged. Source that shifts a
   * value out in a loop then never terminates - and if it allocates per iteration, it does not fail, it exhausts the
   * machine. That is not a diagnosable condition the way an out-of-bounds index is: a distance past the width has one
   * obvious answer, which is that every bit has been shifted out, so this defines it rather than reporting it.
   *
   * The distance is clamped before the shift as well as selected over afterwards, because the shift is emitted on both
   * paths and has to be in range on the one that is discarded too.
   *
   * @param ctx The llvm context to emit into.
   * @param op The shift being emitted; must satisfy @c is_shift_bin_op.
   * @param a The value being shifted.
   * @param b The distance, already the same type as @p a.
   * @return The shifted value, or zero when @p b is at or past the width of @p a.
   */
  auto EmitDefinedShift(
    spp::codegen::LlvmCtx *const ctx,
    const spp::codegen::func_impls::BinOp op,
    llvm::Value *const a,
    llvm::Value *const b)
    -> llvm::Value* {
    const auto uid = spp::utils::Uid();
    const auto ty = a->getType();
    const auto width = llvm::ConstantInt::get(ty, ty->getIntegerBitWidth());
    const auto max = llvm::ConstantInt::get(ty, ty->getIntegerBitWidth() - 1);

    const auto too_wide = ctx->Builder.CreateICmpUGE(b, width, "shift.wide" + uid);
    const auto safe = ctx->Builder.CreateSelect(too_wide, max, b, "shift.safe" + uid);
    const auto raw = op == spp::codegen::func_impls::BinOp::Shl
      ? ctx->Builder.CreateShl(a, safe, "shift.raw" + uid)
      : ctx->Builder.CreateLShr(a, safe, "shift.raw" + uid);

    return ctx->Builder.CreateSelect(too_wide, llvm::ConstantInt::get(ty, 0), raw, "shift.result" + uid);
  }
}

auto spp::codegen::func_impls::simple_create_fn(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ret_ty, Vec<llvm::Type*> const &param_tys)
  -> llvm::Function* {
  if (const auto declared = proto->GetLlvmFunc(); declared != nullptr and declared->Target != nullptr) {
    return declared->Target;
  }

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

auto spp::codegen::func_impls::is_shift_bin_op(
  const BinOp op) -> bool {
  switch (op) {
    case BinOp::Shl:
    case BinOp::LShr: return true;
    default: return false;
  }
}

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
  LlvmCtx *ctx, const BinOp op, llvm::Value *a, llvm::Value *b)
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
    case BinOp::Shl:
    case BinOp::LShr: return EmitDefinedShift(ctx, op, a, b);
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
    default: throw std::runtime_error(std::format("Unsupported BinOp type: {}", name));
  }
  SPP_ASSERT(false);
  return nullptr;
}

auto spp::codegen::func_impls::apply_un_op(
  LlvmCtx *ctx, const UnOp op, llvm::Value *a) -> llvm::Value* {
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
  LlvmCtx *ctx, const ConvOp op, llvm::Value *a, llvm::Type *dest_ty) -> llvm::Value* {
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
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, const BinOp op) -> void {
  // "ty" (per the dispatcher) is always the function's declared RETURN type. For arithmetic ops that's also the
  // operand type ("T, T -> T"). For comparisons the return type is "Bool" (i1), so the *operand* type has to be read
  // off the function's own first parameter instead - "ty" alone can't give us both.
  const auto param0_name = proto->FnParamGroup->GetAllParams()[0]->ExtractName().get();
  const auto param0_type = sm->CurrentScope->GetVarSymbol(param0_name)->Type.get();
  const auto operand_ty = is_cmp_bin_op(op)
    ? GetLlvmType(*sm->CurrentScope->GetTypeSymbol(param0_type), ctx)
    : ty;

  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{operand_ty, operand_ty});

  // A borrowed operand arrives as the address of the value rather than the value. Every comparison takes its two the
  // that way ("eq(this: &T, that: &T)"), where the arithmetic ones take theirs by value ("add(this: T, that: T)"), so
  // an operand is only usable as it arrives when its own parameter says it is - applying the operation to the two
  // addresses instead asks where the operands live rather than what they are, and for two distinct temporaries that
  // folds to a constant.
  const auto params = proto->FnParamGroup->GetAllParams();
  const auto operand_of = [&](llvm::Value *arg, asts::FunctionParameterAst const &param) {
    if (param.Type->GetConvention() == nullptr) { return arg; }
    const auto value_ty = GetLlvmTypeOf(*param.Type->WithoutConvention(), *sm->CurrentScope, ctx);
    return llvm::cast<llvm::Value>(ctx->Builder.CreateLoad(value_ty, arg, "intrinsic.operand"));
  };

  const auto lhs = operand_of(fn->arg_begin(), *params[0]);
  auto rhs = operand_of(fn->arg_begin() + 1, *params[1]);

  // A shift is the one binary operation whose two operands are separately typed in the source ("bit_shl[T, U](this: T,
  // by: U)"), because a shift distance is a count rather than a value of the thing being shifted. Llvm requires both
  // operands of one, so the distance is widened or narrowed to the shifted value's type. Neither direction can lose a
  // meaningful distance: a distance that does not fit in "T" is already past the width being shifted.
  if (is_shift_bin_op(op) and rhs->getType() != lhs->getType()) {
    rhs = ctx->Builder.CreateZExtOrTrunc(rhs, lhs->getType(), "intrinsic.shift.by");
  }
  ctx->Builder.CreateRet(apply_bin_op(ctx, op, lhs, rhs));
}

auto spp::codegen::func_impls::simple_intrinsic_binop_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *, const BinOp op) -> void {
  // "(this: &mut T, that: U) -> Void": "ty" (per the dispatcher) is the declared return type "Void", not "T", so both
  // operand types are read off the parameters. The two are the same type for every operation but a shift, whose
  // distance is separately typed ("bit_shr_assign(&mut self, that: U32)") - so the slot being updated is sized from
  // "this" rather than from "that", or a "&mut U64" would be loaded and stored 32 bits at a time.
  const auto uid = "." + utils::Uid();
  const auto params = proto->FnParamGroup->GetAllParams();
  const auto value_ty = GetLlvmTypeOf(*params[0]->Type->WithoutConvention(), *sm->CurrentScope, ctx);
  const auto operand_ty = GetLlvmTypeOf(*params.Back()->Type->WithoutConvention(), *sm->CurrentScope, ctx);

  const auto void_ty = llvm::Type::getVoidTy(*ctx->Context);
  const auto ptr_ty = llvm::cast<llvm::Type>(llvm::PointerType::get(*ctx->Context, 0));
  const auto fn = simple_create_fn(sm, proto, meta, ctx, void_ty, Vec{ptr_ty, operand_ty});

  const auto lhs = fn->arg_begin();
  auto rhs = llvm::cast<llvm::Value>(fn->arg_begin() + 1);
  const auto loaded_val = ctx->Builder.CreateLoad(value_ty, lhs, "intrinsic.assign.loaded" + uid);

  // As in the by-value form, a shift distance is widened or narrowed to the type being shifted, which llvm requires to
  // match. Neither direction loses a meaningful distance: one that does not fit in the value's type is already past
  // the width being shifted, and "apply_bin_op" defines that case.
  if (is_shift_bin_op(op) and rhs->getType() != value_ty) {
    rhs = ctx->Builder.CreateZExtOrTrunc(rhs, value_ty, "intrinsic.shift.by" + uid);
  }
  const auto result = apply_bin_op(ctx, op, loaded_val, rhs);
  ctx->Builder.CreateStore(result, lhs);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::simple_intrinsic_unop(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, const UnOp op) -> void {
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{ty});
  const auto operand = fn->arg_begin();
  ctx->Builder.CreateRet(apply_un_op(ctx, op, operand));
}

auto spp::codegen::func_impls::simple_intrinsic_unop_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *, const UnOp op)
  -> void {
  // "(this: &mut T) -> Void": "ty" (per the dispatcher) is the declared return type "Void", not "T" - the operand
  // type is read off "this" instead. "this" is itself a "&mut T" reference, but (matching how every other
  // reference-typed symbol in this file - e.g. "self" - is resolved) GetTypeSymbol/GetLlvmType already unwraps the
  // reference down to plain "T", not a raw pointer type.
  const auto uid = "." + utils::Uid();
  const auto this_param = proto->FnParamGroup->GetAllParams()[0];
  const auto operand_ty = GetLlvmTypeOf(*this_param->Type->WithoutConvention(), *sm->CurrentScope, ctx);

  const auto void_ty = llvm::Type::getVoidTy(*ctx->Context);
  const auto ptr_ty = llvm::cast<llvm::Type>(llvm::PointerType::get(*ctx->Context, 0));
  const auto fn = simple_create_fn(sm, proto, meta, ctx, void_ty, Vec{ptr_ty});

  const auto lhs = fn->arg_begin();
  const auto loaded_val = ctx->Builder.CreateLoad(operand_ty, lhs, "intrinsic.assign.loaded" + uid);
  const auto result = apply_un_op(ctx, op, loaded_val);
  ctx->Builder.CreateStore(result, lhs);
  ctx->Builder.CreateRetVoid();
}

namespace {
  /**
   * Whether a conversion is defined for a source and destination pair.
   *
   * @n
   * Each of these operations is only meaningful over part of the space of type pairs - a truncation has to narrow, an
   * extension has to widen, a bit cast has to keep the size - and llvm rejects an instruction built outside it.
   *
   * @param op The conversion being built.
   * @param src The type being converted from.
   * @param dst The type being converted to.
   * @return Whether @p op is defined from @p src to @p dst .
   */
  auto ConvOpIsDefined(
    const spp::codegen::func_impls::ConvOp op,
    llvm::Type const *src,
    llvm::Type const *dst)
    -> bool {
    using ConvOp = spp::codegen::func_impls::ConvOp;
    const auto ints = src->isIntegerTy() and dst->isIntegerTy();
    const auto floats = src->isFloatingPointTy() and dst->isFloatingPointTy();
    switch (op) {
      case ConvOp::Trunc: return ints and src->getIntegerBitWidth() > dst->getIntegerBitWidth();
      case ConvOp::SExt:
      case ConvOp::ZExt: return ints and src->getIntegerBitWidth() < dst->getIntegerBitWidth();
      case ConvOp::FPTrunc: return floats and src->getPrimitiveSizeInBits() > dst->getPrimitiveSizeInBits();
      case ConvOp::FPExt: return floats and src->getPrimitiveSizeInBits() < dst->getPrimitiveSizeInBits();
      case ConvOp::SIToFP:
      case ConvOp::UIToFP: return src->isIntegerTy() and dst->isFloatingPointTy();
      case ConvOp::FPToSI:
      case ConvOp::FPToUI: return src->isFloatingPointTy() and dst->isIntegerTy();
      case ConvOp::BitCast: return src->isPtrOrPtrVectorTy() == dst->isPtrOrPtrVectorTy()
        and (src->isPtrOrPtrVectorTy() or src->getPrimitiveSizeInBits() == dst->getPrimitiveSizeInBits());
      default: std::unreachable();
    }
  }
}

auto spp::codegen::func_impls::simple_intrinsic_conv(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, const ConvOp op) -> void {
  // "ty" (per the dispatcher) is the function's declared RETURN type - the conversion's destination. The source
  // (operand) type is read off the function's own single parameter instead, since conversions genuinely go from one
  // type to a different one (e.g. "S32 -> F64"), unlike every other builder here where operand type == return type.
  const auto param = proto->FnParamGroup->GetAllParams()[0];
  const auto param_sym = sm->CurrentScope->GetVarSymbol(param->ExtractName().get());
  const auto src_ty = GetLlvmType(*sm->CurrentScope->GetTypeSymbol(param_sym->Type.get()), ctx);
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{src_ty});
  const auto operand = fn->arg_begin();

  // A conversion is only defined for some source/destination pairs - a truncation has to narrow, an extension has to
  // widen, a bit cast has to keep the size. An instantiation for a pair outside that is one nothing can call: the
  // conversions are selected by a "case w of { < that_w { utrunc } > that_w { uzext } else { bit_cast } }", and every
  // arm of that gets instantiated for the widths the enclosing instantiation binds, while only the arm the widths
  // choose can ever run. The other arms are given a body that says so, rather than an instruction llvm rejects.
  if (not ConvOpIsDefined(op, src_ty, ty)) {
    EmitRuntimeAbort(ctx, "integer conversion reached for a width pair it is not defined for");
    return;
  }
  ctx->Builder.CreateRet(apply_conv_op(ctx, op, operand, ty));
}

auto spp::codegen::func_impls::simple_intrinsic_is_const(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, const bool is_float, const double value) -> void {
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto operand_ty = GetLlvmTypeOf(*self_sym->Type->WithoutConvention(), *sm->CurrentScope, ctx);
  const auto ptr_ty = llvm::cast<llvm::Type>(llvm::PointerType::get(*ctx->Context, 0));
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{ptr_ty});
  const auto operand = ctx->Builder.CreateLoad(operand_ty, fn->arg_begin(), "intrinsic.operand" + uid);
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

/**
 * Read the value a "cmp" generic parameter of the enclosing function was bound to on this instantiation, as an atomic
 * ordering. Llvm fixes the ordering of an atomic operation when the instruction is built - there is no atomic
 * instruction that takes a runtime ordering - so the orderings are generic parameters of the atomic intrinsics rather
 * than function parameters, and their values are read from the instantiation's symbol. Reading them off the
 * "llvm::Function"'s arguments cannot work: an "llvm::Argument" is never an "llvm::ConstantInt", whatever the caller
 * passed.
 * @param sm The scope manager, positioned on the instantiated function's scope.
 * @param meta The compiler meta data.
 * @param ctx The llvm context to generate the bound value into.
 * @param name The name of the generic parameter holding the ordering.
 * @return The atomic ordering this instantiation was created for.
 */
static auto read_atomic_ordering(
  spp::analyse::scopes::ScopeManager *const sm,
  spp::asts::meta::CompilerMetaData *const meta,
  spp::codegen::LlvmCtx *const ctx,
  spp::Str const &name)
  -> llvm::AtomicOrdering {
  const auto param_name = spp::asts::IdentifierAst(0uz, name);
  const auto order_sym = sm->CurrentScope->GetVarSymbol(&param_name);
  SPP_ASSERT(order_sym != nullptr);

  // A template never reaches code generation, so the parameter is always bound by the time this runs.
  const auto bound = order_sym->BoundCompValue();
  SPP_ASSERT(bound != nullptr);

  ctx->InConstantContext = true;
  const auto order_val = bound->Stage11_CodeGen(sm, meta, ctx);
  ctx->InConstantContext = false;
  return static_cast<llvm::AtomicOrdering>(llvm::cast<llvm::ConstantInt>(order_val)->getZExtValue());
}

auto spp::codegen::func_impls::simple_atomic_fetch_rmw(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, const AtomicRmwOp op) -> void {
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

  // "val" is the only non-"self" parameter; the ordering is a generic parameter.
  const auto val_param = proto->FnParamGroup->GetNonSelfParams()[0];
  const auto val_sym = sm->CurrentScope->GetVarSymbol(val_param->ExtractName().get());
  const auto val_arg = ctx->Builder.CreateLoad(val_ty, val_sym->LlvmInfo->Alloca, "atomic.fetch.operand");

  auto const &dl = ctx->Module->getDataLayout();
  const auto rmw_inst = ctx->Builder.CreateAtomicRMW(
    apply_atomic_rmw_op(op), val_field_ptr, val_arg, dl.getABITypeAlign(val_ty),
    read_atomic_ordering(sm, meta, ctx, "order"));
  ctx->Builder.CreateRet(rmw_inst);
}

namespace {
  /**
   * Whether a resolved sized-integer type is a signed one.
   *
   * @n
   * Not a question the name can answer. @c "U8" is an alias for @c "SizedIntegerUnsigned[8]", which is itself an
   * alias for @c "SizedInteger[8, false]" - so by the time the type is resolved every width of both signednesses is
   * called @c SizedInteger . Reading the first letter made all of them look signed, and @c "max_val[U8]()" came back
   * as 127. The signedness is the type's own @c signed comp argument, so that is what is read; the name is only a
   * fallback for a type that somehow arrives without one.
   *
   * @param type The resolved return type of the intrinsic.
   * @return Whether it is a signed integer.
   */
  auto IsSignedIntegerType(
    spp::asts::TypeAst const &type) -> bool {
    if (auto const *signed_arg = type.LastTypePart()->GnArgGroup->CompAt("signed"); signed_arg != nullptr) {
      auto const *literal = signed_arg->Val->To<spp::asts::BooleanLiteralAst>();
      if (literal != nullptr) { return literal->CppVal(); }
    }
    return type.LastTypePart()->Name.starts_with("S");
  }
}

auto spp::codegen::func_impls::simple_binary_intrinsic_call(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, const llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void {
  const auto uid = "." + utils::Uid();
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{ty, ty});
  const auto lhs = fn->arg_begin();
  const auto rhs = fn->arg_begin() + 1;
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(ctx->Module.get(), intrinsic, {ty});
  const auto result = ctx->Builder.CreateCall(intrinsic_fn, {lhs, rhs}, "intrinsic.result" + uid);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::func_impls::simple_binary_intrinsic_call_overflow(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, const llvm::Intrinsic::IndependentIntrinsics intrinsic)
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

  // The intrinsic hands back an anonymous "{T, i1}", while "(T, Bool)" lowers to the named struct every other tuple of
  // that shape shares. Llvm types are compared by identity, not by layout, so the two are different types however
  // alike they look, and the fields have to be moved across rather than the result returned as it stands.
  auto packed = llvm::cast<llvm::Value>(llvm::UndefValue::get(ret_ty));
  packed = ctx->Builder.CreateInsertValue(
    packed, ctx->Builder.CreateExtractValue(result, {0}, "intrinsic.value" + uid), {0}, "intrinsic.packed" + uid);
  packed = ctx->Builder.CreateInsertValue(
    packed, ctx->Builder.CreateExtractValue(result, {1}, "intrinsic.flag" + uid), {1}, "intrinsic.packed" + uid);
  ctx->Builder.CreateRet(packed);
}

auto spp::codegen::func_impls::simple_unary_intrinsic_call(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, const llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void {
  const auto uid = "." + utils::Uid();
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{ty});
  const auto operand = fn->arg_begin();
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(ctx->Module.get(), intrinsic, {ty});
  const auto result = ctx->Builder.CreateCall(intrinsic_fn, {operand}, "intrinsic.result" + uid);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::func_impls::simple_get_value(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, llvm::Value *val) -> void {
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{ty});
  (void)fn;
  ctx->Builder.CreateRet(val);
}

// =========================================================================================================
// Layer 2b: coroutine-specific shared helpers.
// =========================================================================================================

auto spp::codegen::func_impls::simple_coro_iter(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, const bool reverse, const bool borrow) -> void {
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

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, LlvmCtx *ctx) -> llvm::Value* override {
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

auto spp::codegen::func_impls::simple_coro_view_iter(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, const bool reverse, const bool borrow) -> void {
  // A view is a "{data, length}" pair, and the length is a runtime value, so there is no count to unroll against the
  // way there is for an array. This emits the loop instead, with the suspend point inside the body: the counter is an
  // entry-block alloca, so the coroutine passes give it a frame slot and it holds its value across each suspend.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto i64_ty = llvm::Type::getInt64Ty(*ctx->Context);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "view.iter.self" + uid);

  const auto view_type = self_sym->Type->WithoutConvention();
  const auto view_type_sym = sm->CurrentScope->GetTypeSymbol(view_type.get());
  const auto view_llvm_type = llvm::cast<llvm::StructType>(GetLlvmType(*view_type_sym, ctx));

  const auto elem_type_arg = view_type->LastTypePart()->GnArgGroup->TypeAt("T");
  const auto elem_llvm_type = elem_type_arg != nullptr
    ? GetLlvmTypeOf(*elem_type_arg->Val, *sm->CurrentScope, ctx)
    : llvm::Type::getInt8Ty(*ctx->Context);

  const auto data_idx = GetPhysicalFieldIndex(*view_type_sym->LlvmInfo, 0);
  const auto length_idx = GetPhysicalFieldIndex(*view_type_sym->LlvmInfo, 1);

  // Both fields are read once, up front: the view itself is not modified by iterating it, so re-reading them each
  // time around would only add loads the optimizer has to prove redundant.
  const auto data = ctx->Builder.CreateLoad(
    ptr_ty, ctx->Builder.CreateStructGEP(view_llvm_type, self_ptr, data_idx, "view.iter.data_ptr" + uid),
    "view.iter.data" + uid);
  const auto length = ctx->Builder.CreateLoad(
    i64_ty, ctx->Builder.CreateStructGEP(view_llvm_type, self_ptr, length_idx, "view.iter.length_ptr" + uid),
    "view.iter.length" + uid);

  // Counted down from the length when reversed, so that one counter drives both directions and neither can run past
  // the end: forwards yields index "i" while "i < length", backwards yields index "i - 1" while "i > 0".
  const auto counter = LlvmEntryAlloca(i64_ty, "view.iter.counter" + uid, ctx);
  ctx->Builder.CreateStore(
    reverse ? static_cast<llvm::Value*>(length) : llvm::ConstantInt::get(i64_ty, 0), counter);

  const auto fn = ctx->Builder.GetInsertBlock()->getParent();
  const auto cond_bb = llvm::BasicBlock::Create(*ctx->Context, "view.iter.cond" + uid, fn);
  const auto body_bb = llvm::BasicBlock::Create(*ctx->Context, "view.iter.body" + uid, fn);
  const auto exit_bb = llvm::BasicBlock::Create(*ctx->Context, "view.iter.exit" + uid, fn);

  ctx->Builder.CreateBr(cond_bb);
  ctx->Builder.SetInsertPoint(cond_bb);
  const auto counter_val = ctx->Builder.CreateLoad(i64_ty, counter, "view.iter.i" + uid);
  const auto more = reverse
    ? ctx->Builder.CreateICmpUGT(counter_val, llvm::ConstantInt::get(i64_ty, 0), "view.iter.more" + uid)
    : ctx->Builder.CreateICmpULT(counter_val, length, "view.iter.more" + uid);
  ctx->Builder.CreateCondBr(more, body_bb, exit_bb);

  // The index the current step yields, filled in below and read by the yielded expression when the "gen" runs.
  auto index = static_cast<llvm::Value*>(nullptr);

  struct CustomExpr : asts::ExpressionAst {
    SPP_AST_KEY_FUNCTIONS_DEFAULT_IMPL

    decltype(index) &_Index;
    decltype(data) &_Data;
    decltype(uid) &_Uid;
    llvm::Type *_ElemTy;
    bool _Borrow;

    CustomExpr(
      decltype(index) &index, decltype(data) &data, decltype(uid) &uid, llvm::Type *elem_ty, const bool borrow)
      : _Index(index), _Data(data), _Uid(uid), _ElemTy(elem_ty), _Borrow(borrow) {}

    auto Stage11_CodeGen(ScopeManager *, CompilerMetaData *, LlvmCtx *ctx) -> llvm::Value* override {
      // Indexed over the element type, so one step of the index advances by one element rather than by one byte.
      const auto shift = ctx->Builder.CreateGEP(_ElemTy, _Data, {_Index}, "view.iter.elem_ptr" + _Uid);
      return _Borrow
        ? shift
        : static_cast<llvm::Value*>(ctx->Builder.CreateLoad(_ElemTy, shift, "view.iter.elem" + _Uid));
    }
  };

  const auto mock_gen = MakeUnique<asts::GenExpressionAst>(
    nullptr, nullptr, MakeUnique<CustomExpr>(index, data, uid, elem_llvm_type, borrow));

  ctx->Builder.SetInsertPoint(body_bb);
  const auto next = reverse
    ? ctx->Builder.CreateSub(counter_val, llvm::ConstantInt::get(i64_ty, 1), "view.iter.next" + uid)
    : ctx->Builder.CreateAdd(counter_val, llvm::ConstantInt::get(i64_ty, 1), "view.iter.next" + uid);
  index = reverse ? next : counter_val;

  // Advanced before suspending, so the slot already holds the next step when the caller resumes.
  ctx->Builder.CreateStore(next, counter);
  mock_gen->Stage11_CodeGen(sm, meta, ctx);

  // The "gen" leaves the builder in the block the coroutine resumes into, which is where the loop closes - branching
  // from "body_bb" would put the back edge before the suspend point instead of after it.
  ctx->Builder.CreateBr(cond_bb);
  ctx->Builder.SetInsertPoint(exit_bb);
}

auto spp::codegen::func_impls::simple_coro_non_null_fwd(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx) -> void {
  // The NonNull[T] type can forward to &T/&mut T - modelled as a
  // pointer to the T type, stored within the NonNull[T]. Use two
  // loads, because there are two levels to go through: "NonNull[T]"
  // lowers to a pointer, but this takes it as "&self", so the
  // parameter is a pointer to *that*.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_slot = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.fwd.self");
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_slot, "non_null.fwd.data");

  struct CustomExpr : asts::ExpressionAst {
    decltype(self_ptr) &SelfPtr;

    explicit CustomExpr(
      decltype(self_ptr) &self_ptr)
      : SelfPtr(self_ptr) {}

    SPP_AST_KEY_FUNCTIONS_DEFAULT_IMPL

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, LlvmCtx *ctx) -> llvm::Value* override {
      return SelfPtr;
    }
  };

  const auto mock_gen = MakeUnique<asts::GenExpressionAst>(
    nullptr, nullptr, MakeUnique<CustomExpr>(self_ptr));
  mock_gen->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::codegen::func_impls::simple_coro_view_slice(
  SPP_LLVM_FUNC_INFO,
  LlvmCtx *ctx)
  -> void {
  // To slice a view, we need to GEP in the "from" and
  // "upto" pointers, and return the memory between.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "view.self" + uid);

  // Extract the "from" and "upto" from the symbol's alloca storage in the symbol table. Taken from the parameters
  // without "self", because "self" is a parameter too - indexing the whole group reads "self" as "from" and "from" as
  // "upto", which slices from wherever the view happens to be stored.
  const auto value_params = proto->FnParamGroup->GetNonSelfParams();
  const auto from_param = value_params[0]->ExtractName();
  const auto upto_param = value_params[1]->ExtractName();

  const auto from_alloca = sm->CurrentScope->GetVarSymbol(from_param.get(), true)->LlvmInfo->Alloca;
  const auto upto_alloca = sm->CurrentScope->GetVarSymbol(upto_param.get(), true)->LlvmInfo->Alloca;

  const auto view_type = self_sym->Type->WithoutConvention();
  const auto view_type_sym = sm->CurrentScope->GetTypeSymbol(view_type.get());
  const auto view_llvm_type = llvm::cast<llvm::StructType>(GetLlvmType(*view_type_sym, ctx));

  const auto elem_type_arg = view_type->LastTypePart()->GnArgGroup->TypeAt("T");
  const auto elem_llvm_type = elem_type_arg != nullptr
    ? GetLlvmTypeOf(*elem_type_arg->Val, *sm->CurrentScope, ctx)
    : llvm::Type::getInt8Ty(*ctx->Context);

  const auto data_idx = GetPhysicalFieldIndex(*view_type_sym->LlvmInfo, 0);
  const auto length_idx = GetPhysicalFieldIndex(*view_type_sym->LlvmInfo, 1);

  struct CustomExpr : asts::ExpressionAst {
    SPP_AST_KEY_FUNCTIONS_DEFAULT_IMPL

    decltype(from_alloca) &_FromAlloca;
    decltype(upto_alloca) &_UptoAlloca;
    decltype(self_ptr) &_SelfPtr;
    decltype(uid) &_Uid;
    llvm::StructType *_ViewTy;
    llvm::Type *_ElemTy;
    std::uint32_t _DataIdx;
    std::uint32_t _LengthIdx;

    CustomExpr(
      decltype(from_alloca) &from_alloca, decltype(upto_alloca) &upto_alloca, decltype(self_ptr) &self_ptr,
      decltype(uid) &uid, llvm::StructType *view_ty, llvm::Type *elem_ty, const std::uint32_t data_idx,
      const std::uint32_t length_idx)
      : _FromAlloca(from_alloca), _UptoAlloca(upto_alloca), _SelfPtr(self_ptr), _Uid(uid), _ViewTy(view_ty),
        _ElemTy(elem_ty), _DataIdx(data_idx), _LengthIdx(length_idx) {}

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, LlvmCtx *ctx) -> llvm::Value* override {
      // Read the bounds out of the parameters' storage.
      const auto i64_ty = llvm::Type::getInt64Ty(*ctx->Context);
      const auto from_val = ctx->Builder.CreateLoad(i64_ty, _FromAlloca, "view.slice.from_val" + _Uid);
      const auto upto_val = ctx->Builder.CreateLoad(i64_ty, _UptoAlloca, "view.slice.upto_val" + _Uid);

      // The data the view spans, read out of its first field.
      const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
      const auto self_data_ptr = ctx->Builder.CreateStructGEP(
        _ViewTy, _SelfPtr, _DataIdx, "view.slice.self_data_ptr" + _Uid);
      const auto self_data = ctx->Builder.CreateLoad(ptr_ty, self_data_ptr, "view.slice.self_data" + _Uid);

      // A slice is a new view over the same storage: its data
      // starts "from" elements along, and it is "upto - from"
      // elements long. Indexing is over the element type, so
      // that one index advances by one element rather than by
      // one byte.
      const auto slice_data = ctx->Builder.CreateGEP(
        _ElemTy, self_data, {from_val}, "view.slice.data" + _Uid);
      const auto slice_length = ctx->Builder.CreateSub(upto_val, from_val, "view.slice.length" + _Uid);

      // The coroutine yields "&View[T]", so what leaves here
      // is the address of that new view rather than the view
      // itself. It lives in the frame, which the caller owns
      // for as long as the borrow does.
      const auto slice = LlvmEntryAlloca(_ViewTy, "view.slice.slice" + _Uid, ctx);
      ctx->Builder.CreateStore(
        slice_data, ctx->Builder.CreateStructGEP(_ViewTy, slice, _DataIdx, "view.slice.data_ptr" + _Uid));
      ctx->Builder.CreateStore(
        slice_length, ctx->Builder.CreateStructGEP(_ViewTy, slice, _LengthIdx, "view.slice.length_ptr" + _Uid));

      return slice;
    }
  };

  const auto mock_gen = MakeUnique<asts::GenExpressionAst>(
    nullptr, nullptr, MakeUnique<CustomExpr>(
      from_alloca, upto_alloca, self_ptr, uid,
      view_llvm_type, elem_llvm_type, data_idx, length_idx));
  mock_gen->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::codegen::func_impls::simple_coro_contiguous_fwd(
  SPP_LLVM_FUNC_INFO,
  LlvmCtx *const ctx,
  llvm::Value *const data,
  llvm::Value *const length)
  -> void {
  // The view type is taken from what the coroutine yields rather than rebuilt from the element type, because what is
  // written here has to be the same "View[T]" the caller resolved - a freshly built one has no symbol in this scope.
  const auto uid = "." + utils::Uid();
  auto view_type = proto->ReturnType->WithoutConvention();
  if (const auto yield_arg = view_type->LastTypePart()->GnArgGroup->TypeAt("Yield"); yield_arg != nullptr) {
    view_type = yield_arg->Val->WithoutConvention();
  }

  const auto view_type_sym = sm->CurrentScope->GetTypeSymbol(view_type.get());
  const auto view_llvm_type = llvm::cast<llvm::StructType>(GetLlvmType(*view_type_sym, ctx));
  const auto data_idx = GetPhysicalFieldIndex(*view_type_sym->LlvmInfo, 0);
  const auto length_idx = GetPhysicalFieldIndex(*view_type_sym->LlvmInfo, 1);

  struct CustomExpr final : asts::ExpressionAst {
    SPP_AST_KEY_FUNCTIONS_DEFAULT_IMPL

    llvm::Value *_Data;
    llvm::Value *_Length;
    llvm::StructType *_ViewTy;
    std::uint32_t _DataIdx;
    std::uint32_t _LengthIdx;
    Str _Uid;

    CustomExpr(
      llvm::Value *const data, llvm::Value *const length, llvm::StructType *const view_ty,
      const std::uint32_t data_idx, const std::uint32_t length_idx, Str uid)
      : _Data(data), _Length(length), _ViewTy(view_ty), _DataIdx(data_idx), _LengthIdx(length_idx),
        _Uid(std::move(uid)) {}

    auto Stage11_CodeGen(ScopeManager *, CompilerMetaData *, LlvmCtx *ctx) -> llvm::Value* override {
      const auto view = LlvmEntryAlloca(_ViewTy, "fwd.view" + _Uid, ctx);
      ctx->Builder.CreateStore(
        _Data, ctx->Builder.CreateStructGEP(_ViewTy, view, _DataIdx, "fwd.view.data_ptr" + _Uid));
      ctx->Builder.CreateStore(
        _Length, ctx->Builder.CreateStructGEP(_ViewTy, view, _LengthIdx, "fwd.view.length_ptr" + _Uid));
      return view;
    }
  };

  const auto mock_gen = MakeUnique<asts::GenExpressionAst>(
    nullptr, nullptr, MakeUnique<CustomExpr>(data, length, view_llvm_type, data_idx, length_idx, uid));
  mock_gen->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::codegen::func_impls::simple_coro_array_fwd(
  SPP_LLVM_FUNC_INFO,
  LlvmCtx *const ctx)
  -> void {
  // "Arr[T, n]" lowers to an llvm "[n x T]" held inline, so the view over it is the array's own address paired with
  // the length the type itself carries. Nothing is read out of "self": the borrow it arrives as is already the
  // address of the first element.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "array.fwd.self" + uid);

  const auto arr_type = self_sym->Type->WithoutConvention();
  const auto arr_type_sym = sm->CurrentScope->GetTypeSymbol(arr_type.get());
  const auto arr_llvm_type = llvm::cast<llvm::ArrayType>(GetLlvmType(*arr_type_sym, ctx));

  const auto data = ctx->Builder.CreateConstInBoundsGEP2_64(
    arr_llvm_type, self_ptr, 0, 0, "array.fwd.data" + uid);
  const auto length = llvm::ConstantInt::get(
    llvm::Type::getInt64Ty(*ctx->Context), arr_llvm_type->getNumElements());
  simple_coro_contiguous_fwd(sm, proto, meta, ctx, data, length);
}

auto spp::codegen::func_impls::simple_coro_vector_fwd(
  SPP_LLVM_FUNC_INFO,
  LlvmCtx *const ctx)
  -> void {
  // A vector's elements live in its "buffer", a "RawBuf[T, A]" whose first attribute is the pointer to them. The live
  // region is "[0, length)": "start" only ever moves for the by-value move-iterator, which consumes the vector, so no
  // forwarded view can observe it non-zero.
  using analyse::utils::type_members::GetAllAttrs;
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto i64_ty = llvm::Type::getInt64Ty(*ctx->Context);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "vector.fwd.self" + uid);

  const auto vec_type = self_sym->Type->WithoutConvention();
  const auto vec_type_sym = sm->CurrentScope->GetTypeSymbol(vec_type.get());
  const auto vec_llvm_type = llvm::cast<llvm::StructType>(GetLlvmType(*vec_type_sym, ctx));
  const auto buffer_idx = GetPhysicalFieldIndex(*vec_type_sym->LlvmInfo, 0);
  const auto length_idx = GetPhysicalFieldIndex(*vec_type_sym->LlvmInfo, 1);

  // The buffer lays its own fields out independently of the vector's, so its pointer is reached through its own map
  // rather than assumed to have stayed first.
  const auto buffer_type_sym = spp::get<1>(GetAllAttrs(*vec_type, *sm)[0]);
  const auto buffer_llvm_type = llvm::cast<llvm::StructType>(GetLlvmType(*buffer_type_sym, ctx));
  const auto buffer_ptr_idx = GetPhysicalFieldIndex(*buffer_type_sym->LlvmInfo, 0);

  const auto buffer = ctx->Builder.CreateStructGEP(
    vec_llvm_type, self_ptr, buffer_idx, "vector.fwd.buffer" + uid);
  const auto data = ctx->Builder.CreateLoad(
    ptr_ty, ctx->Builder.CreateStructGEP(buffer_llvm_type, buffer, buffer_ptr_idx, "vector.fwd.data_ptr" + uid),
    "vector.fwd.data" + uid);
  const auto length = ctx->Builder.CreateLoad(
    i64_ty, ctx->Builder.CreateStructGEP(vec_llvm_type, self_ptr, length_idx, "vector.fwd.length_ptr" + uid),
    "vector.fwd.length" + uid);
  simple_coro_contiguous_fwd(sm, proto, meta, ctx, data, length);
}

auto spp::codegen::func_impls::simple_coro_view_index(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx) -> void {
  // To index a view, we need to GEP in the "from" and
  // "upto" pointers, and return the memory between.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "view.self" + uid);

  // Extract the "index" from the symbol's alloca storage in the symbol table. Taken from the parameters without
  // "self", because "self" is a parameter too - indexing the whole group reads "self" as the index, and steps into
  // the view by wherever it happens to be stored.
  const auto idx_param = proto->FnParamGroup->GetNonSelfParams()[0]->ExtractName();
  const auto idx_alloca = sm->CurrentScope->GetVarSymbol(idx_param.get(), true)->LlvmInfo->Alloca;

  // The element being indexed lives in the buffer the view spans, not in the view itself, so the view's own fields
  // have to be read to reach it: its data pointer to step from, its length to check against.
  const auto view_type = self_sym->Type->WithoutConvention();
  const auto view_type_sym = sm->CurrentScope->GetTypeSymbol(view_type.get());
  const auto view_llvm_type = llvm::cast<llvm::StructType>(GetLlvmType(*view_type_sym, ctx));

  const auto elem_type_arg = view_type->LastTypePart()->GnArgGroup->TypeAt("T");
  const auto elem_llvm_type = elem_type_arg != nullptr
    ? GetLlvmTypeOf(*elem_type_arg->Val, *sm->CurrentScope, ctx)
    : llvm::Type::getInt8Ty(*ctx->Context);

  const auto data_idx = GetPhysicalFieldIndex(*view_type_sym->LlvmInfo, 0);
  const auto length_idx = GetPhysicalFieldIndex(*view_type_sym->LlvmInfo, 1);

  struct CustomExpr : asts::ExpressionAst {
    SPP_AST_KEY_FUNCTIONS_DEFAULT_IMPL

    decltype(idx_alloca) &_IdxAlloca;
    decltype(self_ptr) &_SelfPtr;
    decltype(uid) &_Uid;
    llvm::StructType *_ViewTy;
    llvm::Type *_ElemTy;
    std::uint32_t _DataIdx;
    std::uint32_t _LengthIdx;

    CustomExpr(
      decltype(idx_alloca) &idx_alloca, decltype(self_ptr) &self_ptr, decltype(uid) &uid,
      llvm::StructType *view_ty, llvm::Type *elem_ty, const std::uint32_t data_idx, const std::uint32_t length_idx) :
      _IdxAlloca(idx_alloca), _SelfPtr(self_ptr), _Uid(uid), _ViewTy(view_ty), _ElemTy(elem_ty), _DataIdx(data_idx),
      _LengthIdx(length_idx) {}

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, LlvmCtx *ctx) -> llvm::Value* override {
      const auto i64_ty = llvm::Type::getInt64Ty(*ctx->Context);
      const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
      const auto idx_val = ctx->Builder.CreateLoad(i64_ty, _IdxAlloca, "view.index.idx_val" + _Uid);

      // The data the view spans and how much of it there is, read out of the view's own two fields.
      const auto self_data = ctx->Builder.CreateLoad(
        ptr_ty, ctx->Builder.CreateStructGEP(_ViewTy, _SelfPtr, _DataIdx, "view.index.data_ptr" + _Uid),
        "view.index.data" + _Uid);
      const auto self_length = ctx->Builder.CreateLoad(
        i64_ty, ctx->Builder.CreateStructGEP(_ViewTy, _SelfPtr, _LengthIdx, "view.index.length_ptr" + _Uid),
        "view.index.length" + _Uid);

      // Out of bounds aborts rather than returning something: the contract is that an out-of-range index aborts,
      // and "get_ref"/"get_mut" are the accessors that answer with "None" instead. Both numbers are reported, because
      // being told only that one of them was out of range leaves the reader to find both by hand.
      const auto fn = ctx->Builder.GetInsertBlock()->getParent();
      const auto ok_bb = llvm::BasicBlock::Create(*ctx->Context, "view.index.ok" + _Uid, fn);
      const auto oob_bb = llvm::BasicBlock::Create(*ctx->Context, "view.index.oob" + _Uid, fn);
      ctx->Builder.CreateCondBr(
        ctx->Builder.CreateICmpULT(idx_val, self_length, "view.index.in_bounds" + _Uid), ok_bb, oob_bb);

      ctx->Builder.SetInsertPoint(oob_bb);
      EmitRuntimeAbort(ctx, "index %zu out of bounds for length %zu", {idx_val, self_length});

      // Indexed over the element type, so one step of the index advances by one element rather than by one byte.
      ctx->Builder.SetInsertPoint(ok_bb);
      return ctx->Builder.CreateGEP(_ElemTy, self_data, {idx_val}, "view.index.elem_ptr" + _Uid);
    }
  };

  const auto mock_gen = MakeUnique<asts::GenExpressionAst>(
    nullptr, nullptr, MakeUnique<CustomExpr>(
      idx_alloca, self_ptr, uid, view_llvm_type, elem_llvm_type, data_idx, length_idx));
  mock_gen->Stage11_CodeGen(sm, meta, ctx);
}

// =========================================================================================================
// Layer 3: BinOp (simple_intrinsic_binop)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_add(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::Add);
}

auto spp::codegen::func_impls::std_intrinsics_sub(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::Sub);
}

auto spp::codegen::func_impls::std_intrinsics_mul(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::Mul);
}

auto spp::codegen::func_impls::std_intrinsics_sdiv(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::SDiv);
}

auto spp::codegen::func_impls::std_intrinsics_udiv(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::UDiv);
}

auto spp::codegen::func_impls::std_intrinsics_srem(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::SRem);
}

auto spp::codegen::func_impls::std_intrinsics_urem(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::URem);
}

auto spp::codegen::func_impls::std_intrinsics_bit_shl(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::Shl);
}

auto spp::codegen::func_impls::std_intrinsics_bit_shr(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::LShr);
}

auto spp::codegen::func_impls::std_intrinsics_bit_ior(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::Or);
}

auto spp::codegen::func_impls::std_intrinsics_bit_and(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::And);
}

auto spp::codegen::func_impls::std_intrinsics_bit_xor(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::Xor);
}

auto spp::codegen::func_impls::std_intrinsics_eq(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpEQ);
}

auto spp::codegen::func_impls::std_intrinsics_oeq(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FCmpOEQ);
}

auto spp::codegen::func_impls::std_intrinsics_ne(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpNE);
}

auto spp::codegen::func_impls::std_intrinsics_one(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FCmpONE);
}

auto spp::codegen::func_impls::std_intrinsics_slt(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpSLT);
}

auto spp::codegen::func_impls::std_intrinsics_ult(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpULT);
}

auto spp::codegen::func_impls::std_intrinsics_olt(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FCmpOLT);
}

auto spp::codegen::func_impls::std_intrinsics_sle(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpSLE);
}

auto spp::codegen::func_impls::std_intrinsics_ule(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpULE);
}

auto spp::codegen::func_impls::std_intrinsics_ole(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FCmpOLE);
}

auto spp::codegen::func_impls::std_intrinsics_sgt(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpSGT);
}

auto spp::codegen::func_impls::std_intrinsics_ugt(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpUGT);
}

auto spp::codegen::func_impls::std_intrinsics_ogt(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FCmpOGT);
}

auto spp::codegen::func_impls::std_intrinsics_sge(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpSGE);
}

auto spp::codegen::func_impls::std_intrinsics_uge(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::ICmpUGE);
}

auto spp::codegen::func_impls::std_intrinsics_oge(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FCmpOGE);
}

auto spp::codegen::func_impls::std_intrinsics_fadd(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FAdd);
}

auto spp::codegen::func_impls::std_intrinsics_fsub(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FSub);
}

auto spp::codegen::func_impls::std_intrinsics_fmul(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FMul);
}

auto spp::codegen::func_impls::std_intrinsics_fdiv(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FDiv);
}

auto spp::codegen::func_impls::std_intrinsics_frem(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::FRem);
}

auto spp::codegen::func_impls::std_intrinsics_sadd_wrapping(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::Add);
}

auto spp::codegen::func_impls::std_intrinsics_uadd_wrapping(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::Add);
}

auto spp::codegen::func_impls::std_intrinsics_ssub_wrapping(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::Sub);
}

auto spp::codegen::func_impls::std_intrinsics_usub_wrapping(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::Sub);
}

auto spp::codegen::func_impls::std_intrinsics_smul_wrapping(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::Mul);
}

auto spp::codegen::func_impls::std_intrinsics_umul_wrapping(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop(sm, proto, meta, ctx, ty, BinOp::Mul);
}

// =========================================================================================================
// Layer 3: BinOp (simple_intrinsic_binop_assign)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_add_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::Add);
}

auto spp::codegen::func_impls::std_intrinsics_sub_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::Sub);
}

auto spp::codegen::func_impls::std_intrinsics_mul_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::Mul);
}

auto spp::codegen::func_impls::std_intrinsics_sdiv_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::SDiv);
}

auto spp::codegen::func_impls::std_intrinsics_udiv_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::UDiv);
}

auto spp::codegen::func_impls::std_intrinsics_srem_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::SRem);
}

auto spp::codegen::func_impls::std_intrinsics_urem_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::URem);
}

auto spp::codegen::func_impls::std_intrinsics_bit_shl_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::Shl);
}

auto spp::codegen::func_impls::std_intrinsics_bit_shr_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::LShr);
}

auto spp::codegen::func_impls::std_intrinsics_bit_ior_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::Or);
}

auto spp::codegen::func_impls::std_intrinsics_bit_and_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::And);
}

auto spp::codegen::func_impls::std_intrinsics_bit_xor_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::Xor);
}

auto spp::codegen::func_impls::std_intrinsics_fadd_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::FAdd);
}

auto spp::codegen::func_impls::std_intrinsics_fsub_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::FSub);
}

auto spp::codegen::func_impls::std_intrinsics_fmul_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::FMul);
}

auto spp::codegen::func_impls::std_intrinsics_fdiv_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::FDiv);
}

auto spp::codegen::func_impls::std_intrinsics_frem_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_binop_assign(sm, proto, meta, ctx, ty, BinOp::FRem);
}

// =========================================================================================================
// Layer 3: UnOp (simple_intrinsic_unop / simple_intrinsic_unop_assign)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_sneg(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_unop(sm, proto, meta, ctx, ty, UnOp::Neg);
}

auto spp::codegen::func_impls::std_intrinsics_fneg(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_unop(sm, proto, meta, ctx, ty, UnOp::FNeg);
}

auto spp::codegen::func_impls::std_intrinsics_bit_not(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_unop(sm, proto, meta, ctx, ty, UnOp::Not);
}

auto spp::codegen::func_impls::std_intrinsics_bit_not_assign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_unop_assign(sm, proto, meta, ctx, ty, UnOp::Not);
}

// =========================================================================================================
// Layer 3: ConvOp (simple_intrinsic_conv)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_sitofp(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::SIToFP);
}

auto spp::codegen::func_impls::std_intrinsics_uitofp(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::UIToFP);
}

auto spp::codegen::func_impls::std_intrinsics_fptrunc(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::FPTrunc);
}

auto spp::codegen::func_impls::std_intrinsics_strunc(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::Trunc);
}

auto spp::codegen::func_impls::std_intrinsics_utrunc(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::Trunc);
}

auto spp::codegen::func_impls::std_intrinsics_szext(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::SExt);
}

auto spp::codegen::func_impls::std_intrinsics_uzext(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::ZExt);
}

auto spp::codegen::func_impls::std_intrinsics_fpext(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::FPExt);
}

auto spp::codegen::func_impls::std_intrinsics_bit_cast(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::BitCast);
}

auto spp::codegen::func_impls::std_intrinsics_fptosi(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::FPToSI);
}

auto spp::codegen::func_impls::std_intrinsics_fptoui(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_conv(sm, proto, meta, ctx, ty, ConvOp::FPToUI);
}

// =========================================================================================================
// Layer 3: "is this constant" (simple_intrinsic_is_const)
// =========================================================================================================

auto spp::codegen::func_impls::std_num_float_is_zero(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_is_const(sm, proto, meta, ctx, ty, true, 0.0);
}

auto spp::codegen::func_impls::std_num_float_is_one(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_is_const(sm, proto, meta, ctx, ty, true, 1.0);
}

auto spp::codegen::func_impls::std_num_int_is_zero(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_is_const(sm, proto, meta, ctx, ty, false, 0.0);
}

auto spp::codegen::func_impls::std_num_int_is_one(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_intrinsic_is_const(sm, proto, meta, ctx, ty, false, 1.0);
}

// =========================================================================================================
// Layer 3: fixed values (simple_get_value)
// =========================================================================================================

auto spp::codegen::func_impls::std_array_new(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // The array starts out uninitialized rather than zero-filled; callers that need defined contents go through
  // "new_filled"/"fill", which "mem_set" over this value afterwards.
  const auto val = llvm::UndefValue::get(ty);
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_float_neg_one(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantFP::get(ty, -1.0);
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_float_zero(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantFP::get(ty, 0.0);
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_float_one(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantFP::get(ty, 1.0);
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_int_neg_one(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // All-ones bit pattern is "-1" in two's complement, for any width.
  const auto val = llvm::ConstantInt::get(ty, llvm::APInt::getAllOnes(ty->getIntegerBitWidth()));
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_int_zero(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantInt::get(ty, 0);
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_int_one(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantInt::get(ty, 1);
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_num_int_two(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantInt::get(ty, 2);
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_intrinsics_min_val(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // The lowest representable value for this sized-integer type. LLVM integer types carry no sign, so signedness is
  // read off the resolved "Self" return type's name ("S32" vs "U32") instead of "ty".
  const auto is_signed = IsSignedIntegerType(*proto->ReturnType);
  const auto bit_width = ty->getIntegerBitWidth();
  const auto val = llvm::ConstantInt::get(
    ty, is_signed ? llvm::APInt::getSignedMinValue(bit_width) : llvm::APInt::getMinValue(bit_width));
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_intrinsics_max_val(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // The highest representable value for this sized-integer type. See std_intrinsics_min_val for why signedness
  // comes from the return type's name rather than "ty".
  const auto is_signed = IsSignedIntegerType(*proto->ReturnType);
  const auto bit_width = ty->getIntegerBitWidth();
  const auto val = llvm::ConstantInt::get(
    ty, is_signed ? llvm::APInt::getSignedMaxValue(bit_width) : llvm::APInt::getMaxValue(bit_width));
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_intrinsics_fmin_val(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Most negative finite value representable by this float type.
  const auto val = llvm::ConstantFP::get(*ctx->Context, llvm::APFloat::getLargest(ty->getFltSemantics(), true));
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::func_impls::std_intrinsics_fmax_val(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Largest finite value representable by this float type.
  const auto val = llvm::ConstantFP::get(*ctx->Context, llvm::APFloat::getLargest(ty->getFltSemantics(), false));
  simple_get_value(sm, proto, meta, ctx, ty, val);
}

// =========================================================================================================
// Layer 3: raw LLVM intrinsic calls, "(T, T) -> T" (simple_binary_intrinsic_call)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_smax(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::smax);
}

auto spp::codegen::func_impls::std_intrinsics_umax(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::umax);
}

auto spp::codegen::func_impls::std_intrinsics_smin(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::smin);
}

auto spp::codegen::func_impls::std_intrinsics_umin(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::umin);
}

auto spp::codegen::func_impls::std_intrinsics_fpowi(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // "float_powi(base: T, exponent: S32)" raises a float to
  // an *integer* power, so unlike every other binary intrinsic
  // its two operands are different types - and "llvm.powi"
  // is overloaded on both of them, not just the float.
  // Todo: Tidy this up
  const auto uid = "." + utils::Uid();
  const auto i32_ty = llvm::cast<llvm::Type>(llvm::Type::getInt32Ty(*ctx->Context));

  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{ty, i32_ty});
  const auto base = fn->arg_begin();
  const auto exponent = fn->arg_begin() + 1;
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(
    ctx->Module.get(), llvm::Intrinsic::powi, {ty, i32_ty});
  const auto result = ctx->Builder.CreateCall(intrinsic_fn, {base, exponent}, "intrinsic.result" + uid);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::func_impls::std_intrinsics_fpowf(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::pow);
}

auto spp::codegen::func_impls::std_intrinsics_fatan2(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::atan2);
}

auto spp::codegen::func_impls::std_intrinsics_fmax(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::maxnum);
}

auto spp::codegen::func_impls::std_intrinsics_fmin(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::minnum);
}

auto spp::codegen::func_impls::std_intrinsics_fcopysign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::copysign);
}

auto spp::codegen::func_impls::std_intrinsics_sadd_saturating(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::sadd_sat);
}

auto spp::codegen::func_impls::std_intrinsics_uadd_saturating(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::uadd_sat);
}

auto spp::codegen::func_impls::std_intrinsics_ssub_saturating(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::ssub_sat);
}

auto spp::codegen::func_impls::std_intrinsics_usub_saturating(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::usub_sat);
}

auto spp::codegen::func_impls::std_intrinsics_sshl_saturating(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::sshl_sat);
}

auto spp::codegen::func_impls::std_intrinsics_ushl_saturating(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::ushl_sat);
}

// =========================================================================================================
// Layer 3: raw LLVM intrinsic calls, "(T, T) -> (T, Bool)" (simple_binary_intrinsic_call_overflow)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_sadd_overflow(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call_overflow(sm, proto, meta, ctx, ty, llvm::Intrinsic::sadd_with_overflow);
}

auto spp::codegen::func_impls::std_intrinsics_uadd_overflow(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call_overflow(sm, proto, meta, ctx, ty, llvm::Intrinsic::uadd_with_overflow);
}

auto spp::codegen::func_impls::std_intrinsics_ssub_overflow(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call_overflow(sm, proto, meta, ctx, ty, llvm::Intrinsic::ssub_with_overflow);
}

auto spp::codegen::func_impls::std_intrinsics_usub_overflow(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call_overflow(sm, proto, meta, ctx, ty, llvm::Intrinsic::usub_with_overflow);
}

auto spp::codegen::func_impls::std_intrinsics_smul_overflow(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call_overflow(sm, proto, meta, ctx, ty, llvm::Intrinsic::smul_with_overflow);
}

auto spp::codegen::func_impls::std_intrinsics_umul_overflow(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_binary_intrinsic_call_overflow(sm, proto, meta, ctx, ty, llvm::Intrinsic::umul_with_overflow);
}

// =========================================================================================================
// Layer 3: raw LLVM intrinsic calls, "(T) -> T" (simple_unary_intrinsic_call)
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_abs(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::abs);
}

auto spp::codegen::func_impls::std_intrinsics_fsqrt(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::sqrt);
}

auto spp::codegen::func_impls::std_intrinsics_fsin(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::sin);
}

auto spp::codegen::func_impls::std_intrinsics_fcos(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::cos);
}

auto spp::codegen::func_impls::std_intrinsics_ftan(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::tan);
}

auto spp::codegen::func_impls::std_intrinsics_fasin(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::asin);
}

auto spp::codegen::func_impls::std_intrinsics_facos(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::acos);
}

auto spp::codegen::func_impls::std_intrinsics_fatan(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::atan);
}

auto spp::codegen::func_impls::std_intrinsics_fsinh(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::sinh);
}

auto spp::codegen::func_impls::std_intrinsics_fcosh(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::cosh);
}

auto spp::codegen::func_impls::std_intrinsics_ftanh(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::tanh);
}

auto spp::codegen::func_impls::std_intrinsics_fexp(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::exp);
}

auto spp::codegen::func_impls::std_intrinsics_fexp2(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::exp2);
}

auto spp::codegen::func_impls::std_intrinsics_fexp10(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::exp10);
}

auto spp::codegen::func_impls::std_intrinsics_flog(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::log);
}

auto spp::codegen::func_impls::std_intrinsics_flog2(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::log2);
}

auto spp::codegen::func_impls::std_intrinsics_flog10(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::log10);
}

auto spp::codegen::func_impls::std_intrinsics_fabs(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::fabs);
}

auto spp::codegen::func_impls::std_intrinsics_ffloor(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::floor);
}

auto spp::codegen::func_impls::std_intrinsics_fceil(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::ceil);
}

auto spp::codegen::func_impls::std_intrinsics_ftrunc(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::trunc);
}

auto spp::codegen::func_impls::std_intrinsics_fround(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::round);
}

auto spp::codegen::func_impls::std_intrinsics_bitreverse(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::bitreverse);
}

auto spp::codegen::func_impls::std_intrinsics_ctlz(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_unary_intrinsic_call(sm, proto, meta, ctx, ty, llvm::Intrinsic::ctlz);
}

auto spp::codegen::func_impls::std_debug_breakpoint_internal(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  simple_create_fn(sm, proto, meta, ctx, ty, {});
  ctx->Builder.CreateIntrinsic(llvm::Intrinsic::debugtrap, {}, {}, {}, "");
  ctx->Builder.CreateRetVoid();
}

// =========================================================================================================
// Layer 3: three-way integer comparisons. "(this: &T, that: &T) -> S32": "ty" (per the dispatcher) is the return
// type "S32" - "T" is read off "this" instead. "llvm.scmp"/"llvm.ucmp" are overloaded on both the result type and
// the operand type, so both types are passed to "getOrInsertDeclaration".
// =========================================================================================================

auto spp::codegen::func_impls::std_intrinsics_scmp(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Both operands are declared "&T", so they arrive as pointers and the values have to be read out of them before
  // the comparison intrinsic - which takes the integers themselves - can be handed anything.
  const auto this_param = proto->FnParamGroup->GetAllParams()[0];
  const auto operand_ty = GetLlvmTypeOf(*this_param->Type->WithoutConvention(), *sm->CurrentScope, ctx);

  const auto uid = "." + utils::Uid();
  const auto ptr_ty = llvm::cast<llvm::Type>(llvm::PointerType::get(*ctx->Context, 0));
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{ptr_ty, ptr_ty});
  const auto lhs = ctx->Builder.CreateLoad(operand_ty, fn->arg_begin(), "intrinsic.lhs" + uid);
  const auto rhs = ctx->Builder.CreateLoad(operand_ty, fn->arg_begin() + 1, "intrinsic.rhs" + uid);
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(
    ctx->Module.get(), llvm::Intrinsic::scmp, {ty, operand_ty});
  const auto result = ctx->Builder.CreateCall(intrinsic_fn, {lhs, rhs}, "intrinsic.result" + uid);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::func_impls::std_intrinsics_ucmp(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Both operands are declared "&T", so they arrive as pointers and the values have to be read out of them before
  // the comparison intrinsic - which takes the integers themselves - can be handed anything.
  const auto this_param = proto->FnParamGroup->GetAllParams()[0];
  const auto operand_ty = GetLlvmTypeOf(*this_param->Type->WithoutConvention(), *sm->CurrentScope, ctx);

  const auto uid = "." + utils::Uid();
  const auto ptr_ty = llvm::cast<llvm::Type>(llvm::PointerType::get(*ctx->Context, 0));
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{ptr_ty, ptr_ty});
  const auto lhs = ctx->Builder.CreateLoad(operand_ty, fn->arg_begin(), "intrinsic.lhs" + uid);
  const auto rhs = ctx->Builder.CreateLoad(operand_ty, fn->arg_begin() + 1, "intrinsic.rhs" + uid);
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
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // "(value: T, flag: S32) -> Bool"; "ty" (per the dispatcher) is the return type "Bool" (i1) - "T" is read off the
  // "value" parameter instead.
  const auto value_param = proto->FnParamGroup->GetNonSelfParams()[0];
  const auto value_ty = GetLlvmTypeOf(*value_param->Type->WithoutConvention(), *sm->CurrentScope, ctx);

  const auto uid = "." + utils::Uid();
  const auto i32_ty = llvm::cast<llvm::Type>(llvm::Type::getInt32Ty(*ctx->Context));
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{value_ty, i32_ty});
  const auto value_arg = fn->arg_begin();
  const auto flag_arg = fn->arg_begin() + 1;
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(
    ctx->Module.get(), llvm::Intrinsic::is_fpclass, {value_ty});

  static constexpr auto kClassCount = 10u;
  const auto done_bb = llvm::BasicBlock::Create(*ctx->Context, "fpclass.done" + uid, fn);
  const auto other_bb = llvm::BasicBlock::Create(*ctx->Context, "fpclass.other" + uid, fn);
  const auto sw = ctx->Builder.CreateSwitch(flag_arg, other_bb, kClassCount);

  auto incoming = std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>>();
  for (auto i = 0u; i < kClassCount; ++i) {
    const auto case_bb = llvm::BasicBlock::Create(*ctx->Context, "fpclass.c" + std::to_string(i) + uid, fn);
    sw->addCase(llvm::cast<llvm::ConstantInt>(llvm::ConstantInt::get(i32_ty, i)), case_bb);
    ctx->Builder.SetInsertPoint(case_bb);
    const auto hit = ctx->Builder.CreateCall(
      intrinsic_fn, {value_arg, llvm::ConstantInt::get(i32_ty, 1u << i)}, "intrinsic.result" + uid);
    ctx->Builder.CreateBr(done_bb);
    incoming.emplace_back(hit, case_bb);
  }

  ctx->Builder.SetInsertPoint(other_bb);
  ctx->Builder.CreateBr(done_bb);

  ctx->Builder.SetInsertPoint(done_bb);
  const auto result = ctx->Builder.CreatePHI(ty, kClassCount + 1, "intrinsic.result" + uid);
  for (auto const &[val, bb] : incoming) { result->addIncoming(val, bb); }
  result->addIncoming(llvm::ConstantInt::get(ty, 0), other_bb);
  ctx->Builder.CreateRet(result);
}

// =========================================================================================================
// Layer 3: bespoke - coroutines / arrays / vectors / slots / futures / memory / atomics.
// =========================================================================================================

auto spp::codegen::func_impls::std_array_iter_mov(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_iter(sm, proto, meta, ctx, false, false);
}

auto spp::codegen::func_impls::std_array_reverse_iter_mov(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_iter(sm, proto, meta, ctx, true, false);
}

auto spp::codegen::func_impls::std_array_fwd_ref(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  simple_coro_array_fwd(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_array_fwd_mut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  simple_coro_array_fwd(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_vector_fwd_ref(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  simple_coro_vector_fwd(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_vector_fwd_mut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  simple_coro_vector_fwd(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_generator_send(
  SPP_LLVM_FUNC_INFO,
  LlvmCtx *ctx,
  llvm::Type *)
  -> void {
  // Dummy function for analysis. Still needs terminating. The ".res()" operator handles the lowering for generators
  // there, so reaching this body means "send" was called directly rather than through it.
  EmitRuntimeAbort(ctx, "generator 'send' was reached directly; it is lowered through the '.res()' operator");
}

auto spp::codegen::func_impls::std_generator_once_send(
  SPP_LLVM_FUNC_INFO,
  LlvmCtx *ctx,
  llvm::Type *)
  -> void {
  // Dummy function for analysis. Still needs terminating. The ".res()" operator handles the lowering for generators
  // there, so reaching this body means "send" was called directly rather than through it.
  EmitRuntimeAbort(ctx, "generator 'send' was reached directly; it is lowered through the '.res()' operator");
}

auto spp::codegen::func_impls::std_generator_drop(
  SPP_LLVM_FUNC_INFO,
  LlvmCtx *ctx,
  llvm::Type *)
  -> void {
  //
  using asts::generate::common_types_precompiled::SELF_TYPE;

  // A generator is a bare coroutine handle, and destroying one means destroying the frame it refers to - nothing else
  // frees that frame. "self" is taken by move, so its slot holds the handle itself, and that slot is the address the
  // destruction works through. "EmitDrop" is what knows to lower a generator to "llvm.coro.destroy", including the
  // null check for a handle that was never assigned one.
  const auto self_param = proto->FnParamGroup->GetSelfParam();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(self_param->ExtractName().get());
  const auto self_ty_sym = sm->CurrentScope->GetTypeSymbol(SELF_TYPE.get());
  EmitDrop(*self_ty_sym, self_sym->LlvmInfo->Alloca, sm, meta, ctx);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_slot_get_ref(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
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

    explicit CustomExpr(
      decltype(self_ptr) &self_ptr) :
      _SelfPtr(self_ptr) {}

    auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, LlvmCtx *ctx) -> llvm::Value* override {
      return _SelfPtr;
    }
  };

  const auto mock_gen = MakeUnique<asts::GenExpressionAst>(
    nullptr, nullptr, MakeUnique<CustomExpr>(self_ptr));
  mock_gen->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::codegen::func_impls::std_slot_get_mut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  //
  std_slot_get_ref(sm, proto, meta, ctx, ty);
}

auto spp::codegen::func_impls::std_slot_replace(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
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

  const auto new_val_param = proto->FnParamGroup->GetNonSelfParams()[0];
  const auto new_val_sym = sm->CurrentScope->GetVarSymbol(new_val_param->ExtractName().get());
  const auto new_val_ptr = new_val_sym->LlvmInfo->Alloca;

  const auto old_val = ctx->Builder.CreateLoad(val_ty, val_field_ptr, "slot.replace.old" + uid);
  const auto new_val = ctx->Builder.CreateLoad(val_ty, new_val_ptr, "slot.replace.new" + uid);
  ctx->Builder.CreateStore(new_val, val_field_ptr);
  ctx->Builder.CreateRet(old_val);
}

auto spp::codegen::func_impls::std_string_view_slice_ref(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  simple_coro_view_slice(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_string_view_slice_mut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  simple_coro_view_slice(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_view_index_ref(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  simple_coro_view_index(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_view_index_mut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  simple_coro_view_index(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_view_slice_ref(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  simple_coro_view_slice(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_view_slice_mut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  simple_coro_view_slice(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_view_iter_ref(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_view_iter(sm, proto, meta, ctx, false, true);
}

auto spp::codegen::func_impls::std_view_iter_mut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_view_iter(sm, proto, meta, ctx, false, true);
}

auto spp::codegen::func_impls::std_view_reverse_iter_ref(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_view_iter(sm, proto, meta, ctx, true, true);
}

auto spp::codegen::func_impls::std_view_reverse_iter_mut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_view_iter(sm, proto, meta, ctx, true, true);
}

auto spp::codegen::func_impls::std_cffi_c_closure_from(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  // A closure and a "CClosure" are the same two pointers in the same order - the function and the environment it
  // captures - so this reads the pair back out under the other name. Nothing is copied, and nothing can be: the
  // environment's size is not in the closure's type, which is why it is heap allocated where it is built.
  const auto uid = "." + utils::Uid();
  const auto value_param = proto->FnParamGroup->GetNonSelfParams()[0];
  const auto value_sym = sm->CurrentScope->GetVarSymbol(value_param->ExtractName().get());

  // Both sides are two pointers, but they are different named struct types, and llvm holds a return to the exact one
  // the function declares - so the fields are read out of the closure and put back into "CClosure"'s own type.
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto pair_ty = llvm::StructType::get(*ctx->Context, {ptr_ty, ptr_ty});
  const auto pair = ctx->Builder.CreateLoad(pair_ty, value_sym->LlvmInfo->Alloca, "c_closure.from.pair" + uid);

  const auto ret_ty = ctx->Builder.GetInsertBlock()->getParent()->getReturnType();
  auto out = llvm::cast<llvm::Value>(llvm::UndefValue::get(ret_ty));
  out = ctx->Builder.CreateInsertValue(
    out, ctx->Builder.CreateExtractValue(pair, {0}, "c_closure.from.fn" + uid), {0});
  out = ctx->Builder.CreateInsertValue(
    out, ctx->Builder.CreateExtractValue(pair, {1}, "c_closure.from.env" + uid), {1});
  ctx->Builder.CreateRet(out);
}

auto spp::codegen::func_impls::std_function_fun_mov_drop(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  // A closure that captures anything it can carry away holds its environment on the heap, because it may outlive the
  // frame that made it (see "ClosureExpressionAst::Stage11_CodeGen"). Dropping the closure is what releases it. The
  // value is the "{fn, env}" pair, so the environment is the second field.
  //
  // The captures inside it are not destroyed, only the storage holding them. Doing better needs the closure's type to
  // say what it captured, and it does not: every closure with the same signature has the same type, so there is no
  // per-closure destructor to reach from here. Todo: give a closure a type of its own, and drop its captures.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto pair_ty = llvm::StructType::get(*ctx->Context, {ptr_ty, ptr_ty});

  const auto pair = ctx->Builder.CreateLoad(pair_ty, self_sym->LlvmInfo->Alloca, "fun_mov.drop.pair" + uid);
  const auto env = ctx->Builder.CreateExtractValue(pair, {1}, "fun_mov.drop.env" + uid);

  const auto llvm_free = GetEmissionModule(*ctx)->getOrInsertFunction(
    "sppc_free", llvm::FunctionType::get(llvm::Type::getVoidTy(*ctx->Context), {ptr_ty}, false));
  ctx->Builder.CreateCall(llvm_free, {env});
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_non_null_read(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto data_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.read.data_ptr" + uid);

  // Add a Void guard to cover all eventualities of the generic
  // instantiation of intrinsic functions. Use the special return
  // void instruction in this case.
  if (ty == nullptr or ty->isVoidTy()) {
    ctx->Builder.CreateRetVoid();
    return;
  }

  const auto val = ctx->Builder.CreateLoad(ty, data_ptr, "non_null.read.val" + uid);
  ctx->Builder.CreateRet(val);
}

auto spp::codegen::func_impls::std_non_null_write(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.write.self");
  const auto data_ptr = ctx->Builder.CreateLoad(ptr_ty, self_ptr, "non_null.write.data_ptr");

  // Add a Void guard to cover all eventualities of the generic
  // instantiation of intrinsic functions. In this case, a Void
  // generic arg means the param is removed from the signature.
  // Use the special return void instruction in this case.
  const auto value_params = proto->FnParamGroup->GetNonSelfParams();
  if (value_params.IsEmpty()) {
    ctx->Builder.CreateRetVoid();
    return;
  }

  const auto value_param = value_params[0];
  const auto value_sym = sm->CurrentScope->GetVarSymbol(value_param->ExtractName().get());
  const auto value_ty = GetLlvmTypeOf(*value_param->Type->WithoutConvention(), *sm->CurrentScope, ctx);
  const auto value_val = ctx->Builder.CreateLoad(value_ty, value_sym->LlvmInfo->Alloca, "non_null.write.value");

  ctx->Builder.CreateStore(value_val, data_ptr);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_non_null_raw(
  SPP_LLVM_FUNC_INFO,
  LlvmCtx *ctx,
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

auto spp::codegen::func_impls::std_non_null_erase_type(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.erase_type.self");
  ctx->Builder.CreateRet(self_ptr);
}

auto spp::codegen::func_impls::std_non_null_cast(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.cast.self");
  ctx->Builder.CreateRet(self_ptr);
}

auto spp::codegen::func_impls::std_non_null_from_ptr_inner(
  SPP_LLVM_FUNC_INFO,
  LlvmCtx *ctx,
  llvm::Type *ty)
  -> void {
  //
  const auto ptr_param = proto->FnParamGroup->GetAllParams()[0];
  const auto ptr_sym = sm->CurrentScope->GetVarSymbol(ptr_param->ExtractName().get());
  const auto ptr_struct_ty = llvm::cast<llvm::StructType>(
    GetLlvmTypeOf(*ptr_param->Type->WithoutConvention(), *sm->CurrentScope, ctx));
  const auto addr_field_ptr = ctx->Builder.CreateStructGEP(
    ptr_struct_ty, ptr_sym->LlvmInfo->Alloca, 0, "non_null.from_ptr.addr_field");
  const auto addr_val = ctx->Builder.CreateLoad(
    ptr_struct_ty->getElementType(0), addr_field_ptr, "non_null.from_ptr.addr");

  const auto data_ptr = ctx->Builder.CreateIntToPtr(addr_val, ty, "non_null.from_ptr.data_ptr");
  ctx->Builder.CreateRet(data_ptr);
}

auto spp::codegen::func_impls::std_non_null_fwd_mut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_non_null_fwd(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_non_null_fwd_ref(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_non_null_fwd(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_vol_read(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
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
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
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
  const auto new_param = proto->FnParamGroup->GetNonSelfParams()[0];
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
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "vol.replace.self" + uid);
  const auto slot_ty = llvm::cast<llvm::StructType>(
    GetLlvmType(*sm->CurrentScope->GetTypeSymbol(self_sym->Type.get()), ctx));

  const auto val_field_ptr = ctx->Builder.CreateStructGEP(slot_ty, self_ptr, 0, "vol.replace.val_ptr" + uid);
  const auto val_ty = slot_ty->getElementType(0);

  const auto new_val_param = proto->FnParamGroup->GetNonSelfParams()[0];
  const auto new_val_sym = sm->CurrentScope->GetVarSymbol(new_val_param->ExtractName().get());
  const auto new_val_ptr = new_val_sym->LlvmInfo->Alloca;

  const auto old_val = ctx->Builder.CreateLoad(val_ty, val_field_ptr, true, "vol.replace.old" + uid);
  const auto new_val = ctx->Builder.CreateLoad(val_ty, new_val_ptr, true, "vol.replace.new" + uid);
  ctx->Builder.CreateStore(new_val, val_field_ptr, true);
  ctx->Builder.CreateRet(old_val);
}

auto spp::codegen::func_impls::std_raw_buf_index_ref(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_view_index(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_raw_buf_index_mut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_coro_view_index(sm, proto, meta, ctx);
}

auto spp::codegen::func_impls::std_raw_buf_take_at(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  // Bounds-checked "take", which moves the element at an index
  // off the buffer and hands it back as "Some(val)", or "None"
  // when the index is past the buffer's capacity.
  using asts::generate::common_types_precompiled::SELF_VAR;
  using asts::generate::common_types_precompiled::SELF_TYPE;

  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto self_ty_sym = sm->CurrentScope->GetTypeSymbol(SELF_TYPE.get());
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "raw_buf.take_at.self" + uid);

  const auto elem_ty_spp = SelfTypeName(
    *self_ty_sym)->LastTypePart()->GnArgGroup->TypeAt("T")->Val->WithoutConvention();
  const auto elem_ty_sym = sm->CurrentScope->GetTypeSymbol(elem_ty_spp.get(), true);
  const auto elem_ty = GetLlvmType(*elem_ty_sym, ctx);

  // "self" is a parameter like any other, and is reached through
  // its own symbol above, so the declared parameters are counted
  // without it.
  const auto index_param = proto->FnParamGroup->GetNonSelfParams()[0];
  const auto index_sym = sm->CurrentScope->GetVarSymbol(index_param->ExtractName().get());
  const auto usize_ty = GetLlvmTypeOf(*index_param->Type->WithoutConvention(), *sm->CurrentScope, ctx);
  const auto index_val = ctx->Builder.CreateLoad(
    usize_ty, index_sym->LlvmInfo->Alloca, "raw_buf.take_at.index" + uid);

  // The returned "Opt[T]", and the discriminants of its two
  // alternatives. They are asked for by name rather than
  // taken by position, because nothing about the variant
  // guarantees which order its members are declared in.
  const auto opt_ty_spp = proto->ReturnType->WithoutConvention();
  const auto opt_llvm_ty = GetLlvmTypeOf(*opt_ty_spp, *sm->CurrentScope, ctx);
  const auto some_ty_spp = asts::generate::common_types::SomeType(
    proto->PosStart(), mut_shared_cast(elem_ty_spp));
  const auto none_ty_spp = asts::generate::common_types::None(proto->PosStart());
  const auto some_tag = GetVariantIndexOfMember(*opt_ty_spp, *some_ty_spp, *sm->CurrentScope);
  const auto none_tag = GetVariantIndexOfMember(*opt_ty_spp, *none_ty_spp, *sm->CurrentScope);
  SPP_ASSERT(some_tag.has_value() and none_tag.has_value());

  const auto self_llvm_ty = llvm::cast<llvm::StructType>(GetLlvmType(*self_ty_sym, ctx));
  const auto capacity_idx = GetPhysicalFieldIndex(*self_ty_sym->LlvmInfo, 1);
  const auto capacity = ctx->Builder.CreateLoad(
    usize_ty,
    ctx->Builder.CreateStructGEP(self_llvm_ty, self_ptr, capacity_idx, "raw_buf.take_at.capacity.ptr" + uid),
    "raw_buf.take_at.capacity" + uid);

  const auto fn = ctx->Builder.GetInsertBlock()->getParent();
  const auto in_bounds_bb = llvm::BasicBlock::Create(
    *ctx->Context, "raw_buf.take_at.in_bounds" + uid, fn);
  const auto out_of_bounds_bb = llvm::BasicBlock::Create(
    *ctx->Context, "raw_buf.take_at.out_of_bounds" + uid, fn);
  ctx->Builder.CreateCondBr(
    ctx->Builder.CreateICmpULT(index_val, capacity, "raw_buf.take_at.in_range" + uid),
    in_bounds_bb, out_of_bounds_bb);

  ctx->Builder.SetInsertPoint(in_bounds_bb);
  const auto data_idx = GetPhysicalFieldIndex(*self_ty_sym->LlvmInfo, 0);
  const auto buf_ptr = ctx->Builder.CreateLoad(
    ptr_ty,
    ctx->Builder.CreateStructGEP(self_llvm_ty, self_ptr, data_idx, "raw_buf.take_at.buf.ptr" + uid),
    "raw_buf.take_at.buf" + uid);
  const auto elem_val = ctx->Builder.CreateLoad(
    elem_ty,
    ctx->Builder.CreateGEP(elem_ty, buf_ptr, index_val, "raw_buf.take_at.elem.ptr" + uid),
    "raw_buf.take_at.elem" + uid);
  ctx->Builder.CreateRet(
    BuildVariant(elem_val, opt_llvm_ty, *some_tag, "raw_buf.take_at.some" + uid, ctx));

  ctx->Builder.SetInsertPoint(out_of_bounds_bb);
  ctx->Builder.CreateRet(
    BuildVariant(nullptr, opt_llvm_ty, *none_tag, "raw_buf.take_at.none" + uid, ctx));
}

auto spp::codegen::func_impls::std_raw_buf_place_at(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  using asts::generate::common_types_precompiled::SELF_VAR;
  using asts::generate::common_types_precompiled::SELF_TYPE;
  //
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto self_ty_sym = sm->CurrentScope->GetTypeSymbol(SELF_TYPE.get());
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "vol.replace.self" + uid);

  const auto elem_ty_spp = SelfTypeName(*self_ty_sym)->LastTypePart()->GnArgGroup->TypeAt("T")->Val
                                                     ->WithoutConvention();
  const auto elem_ty_sym = sm->CurrentScope->GetTypeSymbol(elem_ty_spp.get(), true);
  const auto elem_ty = GetLlvmType(*elem_ty_sym, ctx);

  // "self" is a parameter like any other, and is reached through its own symbol above, so the declared parameters
  // are counted without it.
  const auto index_param = proto->FnParamGroup->GetNonSelfParams()[0];
  const auto index_sym = sm->CurrentScope->GetVarSymbol(index_param->ExtractName().get());
  const auto usize_ty = GetLlvmTypeOf(*index_param->Type->WithoutConvention(), *sm->CurrentScope, ctx);
  const auto index_val = ctx->Builder.CreateLoad(usize_ty, index_sym->LlvmInfo->Alloca, "raw_buf.place_at.index");

  const auto element_param = proto->FnParamGroup->GetNonSelfParams()[1];
  const auto element_sym = sm->CurrentScope->GetVarSymbol(element_param->ExtractName().get());
  const auto element_val = ctx->Builder.CreateLoad(elem_ty, element_sym->LlvmInfo->Alloca, "raw_buf.place_at.element");

  const auto self_llvm_ty = llvm::cast<llvm::StructType>(GetLlvmType(*self_ty_sym, ctx));
  const auto data_idx = GetPhysicalFieldIndex(*self_ty_sym->LlvmInfo, 0);
  const auto buf_ptr = ctx->Builder.CreateLoad(
    ptr_ty, ctx->Builder.CreateStructGEP(self_llvm_ty, self_ptr, data_idx, "raw_buf.place_at.buf_ptr" + uid),
    "raw_buf.place_at.buf" + uid);

  const auto elem_addr = ctx->Builder.CreateGEP(elem_ty, buf_ptr, index_val, "raw_buf.place_at.elem_addr");
  ctx->Builder.CreateStore(element_val, elem_addr);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_raw_buf_shift(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  using asts::generate::common_types_precompiled::SELF_TYPE;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto self_ty_sym = sm->CurrentScope->GetTypeSymbol(SELF_TYPE.get());
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "vol.replace.self" + uid);

  const auto elem_ty_spp = SelfTypeName(*self_ty_sym)->LastTypePart()->GnArgGroup->TypeAt("T")->Val
                                                     ->WithoutConvention();
  const auto elem_ty_sym = sm->CurrentScope->GetTypeSymbol(elem_ty_spp.get(), true);
  const auto elem_ty = GetLlvmType(*elem_ty_sym, ctx);

  const auto from_param = proto->FnParamGroup->GetNonSelfParams()[0];
  const auto upto_param = proto->FnParamGroup->GetNonSelfParams()[1];
  const auto count_param = proto->FnParamGroup->GetNonSelfParams()[2];
  const auto from_sym = sm->CurrentScope->GetVarSymbol(from_param->ExtractName().get());
  const auto upto_sym = sm->CurrentScope->GetVarSymbol(upto_param->ExtractName().get());
  const auto count_sym = sm->CurrentScope->GetVarSymbol(count_param->ExtractName().get());
  const auto usize_ty = GetLlvmTypeOf(*from_param->Type->WithoutConvention(), *sm->CurrentScope, ctx);
  const auto from_val = ctx->Builder.CreateLoad(usize_ty, from_sym->LlvmInfo->Alloca, "raw_buf.shift.from");
  const auto upto_val = ctx->Builder.CreateLoad(usize_ty, upto_sym->LlvmInfo->Alloca, "raw_buf.shift.upto");
  const auto count_val = ctx->Builder.CreateLoad(usize_ty, count_sym->LlvmInfo->Alloca, "raw_buf.shift.count");

  const auto self_llvm_ty = llvm::cast<llvm::StructType>(GetLlvmType(*self_ty_sym, ctx));
  const auto data_idx = GetPhysicalFieldIndex(*self_ty_sym->LlvmInfo, 0);
  const auto buf_ptr = ctx->Builder.CreateLoad(
    ptr_ty, ctx->Builder.CreateStructGEP(self_llvm_ty, self_ptr, data_idx, "raw_buf.shift.buf_ptr" + uid),
    "raw_buf.shift.buf" + uid);

  const auto src_addr = ctx->Builder.CreateGEP(elem_ty, buf_ptr, from_val, "raw_buf.shift.src");
  const auto dst_addr = ctx->Builder.CreateGEP(elem_ty, buf_ptr, upto_val, "raw_buf.shift.dst");

  auto const &dl = ctx->Module->getDataLayout();
  const auto elem_size = dl.getTypeAllocSize(elem_ty).getFixedValue();
  const auto elem_align = dl.getABITypeAlign(elem_ty);
  const auto byte_count = ctx->Builder.CreateMul(
    count_val, llvm::ConstantInt::get(usize_ty, elem_size), "raw_buf.shift.bytes");

  ctx->Builder.CreateMemMove(dst_addr, elem_align, src_addr, elem_align, byte_count);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_raw_buf_clear_range(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  using asts::generate::common_types_precompiled::SELF_TYPE;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->GetVarSymbol(SELF_VAR.get(), true);
  const auto self_ty_sym = sm->CurrentScope->GetTypeSymbol(SELF_TYPE.get());
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "raw_buf.clear.self" + uid);

  const auto elem_ty_spp = SelfTypeName(*self_ty_sym)->LastTypePart()->GnArgGroup->TypeAt("T")->Val
                                                     ->WithoutConvention();
  const auto elem_ty_sym = sm->CurrentScope->GetTypeSymbol(elem_ty_spp.get(), true);

  // An element that owns nothing has no destructor to run,
  // so the whole loop collapses to nothing rather than to
  // a loop with an empty body.
  if (not analyse::utils::drop_utils::NeedsDrop(*elem_ty_sym, *sm, meta)) {
    ctx->Builder.CreateRetVoid();
    return;
  }

  const auto elem_ty = GetLlvmType(*elem_ty_sym, ctx);
  const auto start_param = proto->FnParamGroup->GetNonSelfParams()[0];
  const auto count_param = proto->FnParamGroup->GetNonSelfParams()[1];
  const auto start_sym = sm->CurrentScope->GetVarSymbol(start_param->ExtractName().get());
  const auto count_sym = sm->CurrentScope->GetVarSymbol(count_param->ExtractName().get());
  const auto usize_ty = GetLlvmTypeOf(*start_param->Type->WithoutConvention(), *sm->CurrentScope, ctx);
  const auto start_val = ctx->Builder.CreateLoad(usize_ty, start_sym->LlvmInfo->Alloca, "raw_buf.clear.start");
  const auto count_val = ctx->Builder.CreateLoad(usize_ty, count_sym->LlvmInfo->Alloca, "raw_buf.clear.count");

  const auto self_llvm_ty = llvm::cast<llvm::StructType>(GetLlvmType(*self_ty_sym, ctx));
  const auto data_idx = GetPhysicalFieldIndex(*self_ty_sym->LlvmInfo, 0);
  const auto buf_ptr = ctx->Builder.CreateLoad(
    ptr_ty, ctx->Builder.CreateStructGEP(self_llvm_ty, self_ptr, data_idx, "raw_buf.clear.buf_ptr" + uid),
    "raw_buf.clear.buf" + uid);

  // Walk "[start, start + count)" one element at a time,
  // destroying each in place. The counter runs from zero
  // rather than from "start" so the exit test is against
  // "count" directly.
  const auto func = ctx->Builder.GetInsertBlock()->getParent();
  const auto cond_bb = llvm::BasicBlock::Create(*ctx->Context, "raw_buf.clear.cond" + uid, func);
  const auto body_bb = llvm::BasicBlock::Create(*ctx->Context, "raw_buf.clear.body" + uid, func);
  const auto done_bb = llvm::BasicBlock::Create(*ctx->Context, "raw_buf.clear.done" + uid, func);
  const auto entry_bb = ctx->Builder.GetInsertBlock();
  ctx->Builder.CreateBr(cond_bb);

  ctx->Builder.SetInsertPoint(cond_bb);
  const auto index = ctx->Builder.CreatePHI(usize_ty, 2, "raw_buf.clear.i" + uid);
  index->addIncoming(llvm::ConstantInt::get(usize_ty, 0), entry_bb);
  ctx->Builder.CreateCondBr(
    ctx->Builder.CreateICmpULT(index, count_val, "raw_buf.clear.more" + uid), body_bb, done_bb);

  ctx->Builder.SetInsertPoint(body_bb);
  const auto elem_index = ctx->Builder.CreateAdd(start_val, index, "raw_buf.clear.idx" + uid);
  const auto elem_addr = ctx->Builder.CreateGEP(elem_ty, buf_ptr, elem_index, "raw_buf.clear.elem" + uid);
  EmitDrop(*elem_ty_sym, elem_addr, sm, meta, ctx);
  const auto next = ctx->Builder.CreateAdd(index, llvm::ConstantInt::get(usize_ty, 1), "raw_buf.clear.next" + uid);
  index->addIncoming(next, ctx->Builder.GetInsertBlock());
  ctx->Builder.CreateBr(cond_bb);

  ctx->Builder.SetInsertPoint(done_bb);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_mem_ops_size_of(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  //
  const auto t_ast = asts::TypeIdentifierAst::FromString("T");
  const auto t_sym = sm->CurrentScope->GetTypeSymbol(t_ast.get());
  const auto size_val = llvm::ConstantInt::get(ty, SizeOf(*sm, *t_sym->FqName()));
  ctx->Builder.CreateRet(size_val);
}

auto spp::codegen::func_impls::std_mem_ops_align_of(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  //
  const auto t_ast = asts::TypeIdentifierAst::FromString("T");
  const auto t_sym = sm->CurrentScope->GetTypeSymbol(t_ast.get());
  const auto align_val = llvm::ConstantInt::get(ty, AlignOf(*sm, *t_sym->FqName()));
  ctx->Builder.CreateRet(align_val);
}

auto spp::codegen::func_impls::std_mem_ops_size_of_val(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  // Todo: this needs heap querying
  const auto value_param = proto->FnParamGroup->GetAllParams()[0];
  const auto value_sym = sm->CurrentScope->GetVarSymbol(value_param->ExtractName().get());
  const auto elem_type_ast = value_sym->Type->WithoutConvention();
  const auto size_val = llvm::ConstantInt::get(ty, SizeOf(*sm, *elem_type_ast));
  ctx->Builder.CreateRet(size_val);
}

auto spp::codegen::func_impls::std_mem_ops_align_of_val(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  // Todo: this needs heap querying
  const auto value_param = proto->FnParamGroup->GetAllParams()[0];
  const auto value_sym = sm->CurrentScope->GetVarSymbol(value_param->ExtractName().get());
  const auto elem_type_ast = value_sym->Type->WithoutConvention();
  const auto align_val = llvm::ConstantInt::get(ty, AlignOf(*sm, *elem_type_ast));
  ctx->Builder.CreateRet(align_val);
}

auto spp::codegen::func_impls::std_mem_ops_replace(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
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

auto spp::codegen::func_impls::std_mem_ops_drop(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  // "val" is taken by move, so its slot holds the value itself rather than an address of one elsewhere, and that slot
  // is what the destruction works through. This is the owning counterpart of "drop_in_place": the value is consumed by
  // being passed in, so nothing is left behind in the caller for the destroyed storage to be read back out of.
  const auto val_param = proto->FnParamGroup->GetAllParams()[0];
  const auto val_sym = sm->CurrentScope->GetVarSymbol(val_param->ExtractName().get());

  const auto t_ast = asts::TypeIdentifierAst::FromString("T");
  const auto t_sym = sm->CurrentScope->GetTypeSymbol(t_ast.get());
  EmitDrop(*t_sym, val_sym->LlvmInfo->Alloca, sm, meta, ctx);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_mem_ops_drop_in_place(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  // "ptr" is "&mut T" - a borrow, so its slot holds the
  // address of the value rather than the value (see
  // "std_mem_ops_replace" for the same shape). That
  // address is what the value is destroyed through.
  const auto ptr_param = proto->FnParamGroup->GetAllParams()[0];
  const auto ptr_sym = sm->CurrentScope->GetVarSymbol(ptr_param->ExtractName().get());
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto target_ptr = ctx->Builder.CreateLoad(ptr_ty, ptr_sym->LlvmInfo->Alloca, "drop_in_place.ptr");

  // The pointee type is the "T" this instantiation was
  // made for.
  const auto t_ast = asts::TypeIdentifierAst::FromString("T");
  const auto t_sym = sm->CurrentScope->GetTypeSymbol(t_ast.get());
  EmitDrop(*t_sym, target_ptr, sm, meta, ctx);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_threading_atomic_is_lock_free(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_TYPE;

  const auto self_type_sym = sm->CurrentScope->GetTypeSymbol(SELF_TYPE.get());
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
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  // Create the fence function.
  const auto void_ty = llvm::Type::getVoidTy(*ctx->Context);
  simple_create_fn(sm, proto, meta, ctx, void_ty, Vec<llvm::Type*>{});

  // Build the function body.
  ctx->Builder.CreateFence(read_atomic_ordering(sm, meta, ctx, "order"));
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_threading_atomic_load_inner(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  //
  const auto uid = "." + utils::Uid();
  const auto ptr_ty = llvm::cast<llvm::Type>(llvm::PointerType::get(*ctx->Context, 0));
  const auto fn = simple_create_fn(sm, proto, meta, ctx, ty, Vec{ptr_ty});

  const auto ptr_arg = fn->arg_begin();

  const auto load_inst = ctx->Builder.CreateLoad(ty, ptr_arg, "atomic.load" + uid);
  load_inst->setAtomic(read_atomic_ordering(sm, meta, ctx, "order"));
  ctx->Builder.CreateRet(load_inst);
}

auto spp::codegen::func_impls::std_threading_atomic_store_inner(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  //
  const auto val_param = proto->FnParamGroup->GetAllParams()[1];
  const auto val_ty = GetLlvmTypeOf(*val_param->Type->WithoutConvention(), *sm->CurrentScope, ctx);

  const auto void_ty = llvm::Type::getVoidTy(*ctx->Context);
  const auto ptr_ty = llvm::cast<llvm::Type>(llvm::PointerType::get(*ctx->Context, 0));
  const auto fn = simple_create_fn(sm, proto, meta, ctx, void_ty, Vec{ptr_ty, val_ty});

  const auto ptr_arg = fn->arg_begin();
  const auto val_arg = fn->arg_begin() + 1;

  const auto store_inst = ctx->Builder.CreateStore(val_arg, ptr_arg);
  store_inst->setAtomic(read_atomic_ordering(sm, meta, ctx, "order"));
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::func_impls::std_threading_atomic_compex_inner(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  //
  const auto ret_ty = llvm::cast<llvm::StructType>(ty);
  const auto elem_ty = ret_ty->getElementType(0);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto fn = simple_create_fn(
    sm, proto, meta, ctx, ret_ty, Vec<llvm::Type*>{ptr_ty, elem_ty, elem_ty});

  const auto ptr_arg = fn->arg_begin();
  const auto old_arg = fn->arg_begin() + 1;
  const auto new_arg = fn->arg_begin() + 2;

  auto const &dl = ctx->Module->getDataLayout();
  const auto cmpxchg_inst = ctx->Builder.CreateAtomicCmpXchg(
    ptr_arg, old_arg, new_arg, dl.getABITypeAlign(elem_ty),
    read_atomic_ordering(sm, meta, ctx, "success_order"),
    read_atomic_ordering(sm, meta, ctx, "failure_order"));

  // Repack
  const auto uid = "." + utils::Uid();
  auto packed = llvm::cast<llvm::Value>(llvm::UndefValue::get(ret_ty));
  packed = ctx->Builder.CreateInsertValue(
    packed, ctx->Builder.CreateExtractValue(cmpxchg_inst, {0}, "compex.value" + uid), {0}, "compex.packed" + uid);
  packed = ctx->Builder.CreateInsertValue(
    packed, ctx->Builder.CreateExtractValue(cmpxchg_inst, {1}, "compex.flag" + uid), {1}, "compex.packed" + uid);
  ctx->Builder.CreateRet(packed);
}

auto spp::codegen::func_impls::std_threading_atomic_compex_weak_inner(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  //
  const auto ret_ty = llvm::cast<llvm::StructType>(ty);
  const auto elem_ty = ret_ty->getElementType(0);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto fn = simple_create_fn(
    sm, proto, meta, ctx, ret_ty, Vec<llvm::Type*>{ptr_ty, elem_ty, elem_ty});

  const auto ptr_arg = fn->arg_begin();
  const auto old_arg = fn->arg_begin() + 1;
  const auto new_arg = fn->arg_begin() + 2;

  auto const &dl = ctx->Module->getDataLayout();
  const auto cmpxchg_inst = ctx->Builder.CreateAtomicCmpXchg(
    ptr_arg, old_arg, new_arg, dl.getABITypeAlign(elem_ty),
    read_atomic_ordering(sm, meta, ctx, "success_order"),
    read_atomic_ordering(sm, meta, ctx, "failure_order"));
  cmpxchg_inst->setWeak(true);

  // Repack
  const auto uid = "." + utils::Uid();
  auto packed = llvm::cast<llvm::Value>(llvm::UndefValue::get(ret_ty));
  packed = ctx->Builder.CreateInsertValue(
    packed, ctx->Builder.CreateExtractValue(cmpxchg_inst, {0}, "compex.value" + uid), {0}, "compex.packed" + uid);
  packed = ctx->Builder.CreateInsertValue(
    packed, ctx->Builder.CreateExtractValue(cmpxchg_inst, {1}, "compex.flag" + uid), {1}, "compex.packed" + uid);
  ctx->Builder.CreateRet(packed);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_exchange(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::Xchg);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_and(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::And);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_nand(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::Nand);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_or(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::Or);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_xor(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::Xor);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_not(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
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

  auto const &dl = ctx->Module->getDataLayout();
  const auto rmw_inst = ctx->Builder.CreateAtomicRMW(
    apply_atomic_rmw_op(AtomicRmwOp::Xor), val_field_ptr, val_arg, dl.getABITypeAlign(val_ty),
    read_atomic_ordering(sm, meta, ctx, "order"));
  ctx->Builder.CreateRet(rmw_inst);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_add(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::Add);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_sub(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::Sub);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_fadd(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::FAdd);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_fsub(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::FSub);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_fmax(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::FMax);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_fmin(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::FMin);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_smax(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::Max);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_umax(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::UMax);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_smin(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::Min);
}

auto spp::codegen::func_impls::std_threading_atomic_fetch_umin(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  simple_atomic_fetch_rmw(sm, proto, meta, ctx, AtomicRmwOp::UMin);
}

#pragma GCC diagnostic pop
