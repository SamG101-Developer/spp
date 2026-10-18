module;
#include <spp/macros.hpp>

module spp.codegen.llvm_fn_impls;
import spp.analyse.scopes.comp_key;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.drop_utils;
import spp.analyse.utils.type_members;
import spp.asts.ast_kind;
import spp.asts.boolean_literal_ast;
import spp.asts.coroutine_prototype_ast;
import spp.asts.function_parameter_ast;
import spp.asts.function_parameter_group_ast;
import spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_variadic_ast;
import spp.asts.function_prototype_ast;
import spp.asts.gen_expression_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.generate.common_types;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_coros;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_drop;
import spp.codegen.llvm_fn;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_mangle;
import spp.codegen.llvm_size;
import spp.codegen.llvm_type;
import spp.codegen.llvm_variant;
import spp.utils.ice;
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
  /// [CHECKED]
  /// The runtime checks the compiler emits. The numbers are
  /// the runtime's own (SPPC_CHECK_* enum), which names each
  /// check in its report.
  enum class RuntimeCheck : std::uint32_t {
    Index = 1,
    Conversion = 2,
    GeneratorSend = 3,
    SliceOrder = 4,
    SliceEnd = 5,
    FloatToInt = 6,
    FfiUnwind = 7,
    SignedAdd = 0x10,
    UnsignedAdd = 0x11,
    SignedSub = 0x12,
    UnsignedSub = 0x13,
    SignedMul = 0x14,
    UnsignedMul = 0x15,
  };

  /// [CHECKED]
  /// Fail a runtime check, terminating the current block. Every
  /// failed check ends the process through 1 unified runtime
  /// call "sppc_check_failed", which reports the check by name
  /// with the values given and prints a backtrace with "_exit"
  /// (no allocation, no signal handler, etc).
  auto EmitCheckFailed(
    LlvmCtx *const ctx, const RuntimeCheck check,
    llvm::Value *a = nullptr, llvm::Value *b = nullptr) -> void {
    // Get the module and create some predefined types that will
    // be reused through this function.
    const auto mod = ctx->Builder.GetInsertBlock()->getParent()->getParent();
    const auto i32_ty = llvm::Type::getInt32Ty(*ctx->Context);
    const auto i64_ty = llvm::Type::getInt64Ty(*ctx->Context);
    const auto void_ty = llvm::Type::getVoidTy(*ctx->Context);

    // Get the call target embedded in the std library via the
    // sppc ffi bindings.
    auto callee = mod->getOrInsertFunction(
      "sppc_check_failed",
      llvm::FunctionType::get(void_ty, {i32_ty, i64_ty, i64_ty}, false));

    // Assuming the function has been found, set a few llvm attrs
    // on it, for optimization.
    if (const auto fn = llvm::dyn_cast<llvm::Function>(callee.getCallee())) {
      fn->setDoesNotReturn();
      fn->setDoesNotThrow();
      fn->addFnAttr(llvm::Attribute::Cold);
    }

    // Create a call to the check-failed function, passing in the
    // "a" and "b" attributes if they are present, otherwise using
    // zero as fallback defaults.
    const auto zero = llvm::ConstantInt::get(i64_ty, 0);
    ctx->Builder.CreateCall(
      callee, {
        llvm::ConstantInt::get(i32_ty, std::to_underlying(check)),
        a != nullptr ? ctx->Builder.CreateZExtOrTrunc(a, i64_ty) : zero,
        b != nullptr ? ctx->Builder.CreateZExtOrTrunc(b, i64_ty) : zero
      });
    ctx->Builder.CreateUnreachable();
  }

  /// Fail the check unless "value", truncated toward zero, is
  /// in the integer type's range: anything else (too large,
  /// an infinity, NaN) is poison to "fptosi"/"fptoui", so it
  /// aborts like an overflowing "+" rather than converting to
  /// a value llvm may assume is anything. The bounds are the
  /// powers of two around the range, exact in every float
  /// format unless past its largest, where they round to an
  /// infinity and the lower compare is made strict to keep
  /// that infinity out.
  auto EmitFloatToIntCheck(
    LlvmCtx *const ctx, llvm::Value *value, llvm::Type *int_ty, const bool is_signed) -> void {
    const auto uid = "." + spp::utils::Uid();
    const auto i64_ty = llvm::Type::getInt64Ty(*ctx->Context);
    const auto float_ty = value->getType();
    const auto &sem = float_ty->getFltSemantics();
    const auto width = int_ty->getIntegerBitWidth();
    const auto power = [&](const bool negative, const unsigned exp) {
      auto result = llvm::APFloat(sem);
      result.convertFromAPInt(
        llvm::APInt::getOneBitSet(width + 1, exp), false, llvm::APFloat::rmNearestTiesToEven);
      if (negative) { result.changeSign(); }
      return result;
    };
    const auto lo = is_signed ? power(true, width - 1) : llvm::APFloat::getZero(sem);
    const auto hi = power(false, is_signed ? width - 1 : width);

    // Ordered compares, so NaN fails both.
    const auto truncated = ctx->Builder.CreateUnaryIntrinsic(
      llvm::Intrinsic::trunc, value, nullptr, "conv.float.trunc" + uid);
    const auto lo_val = llvm::ConstantFP::get(float_ty, lo);
    const auto lo_ok = lo.isInfinity()
      ? ctx->Builder.CreateFCmpOGT(truncated, lo_val, "conv.float.lo_ok" + uid)
      : ctx->Builder.CreateFCmpOGE(truncated, lo_val, "conv.float.lo_ok" + uid);
    const auto hi_ok = ctx->Builder.CreateFCmpOLT(
      truncated, llvm::ConstantFP::get(float_ty, hi), "conv.float.hi_ok" + uid);

    const auto fn = ctx->Builder.GetInsertBlock()->getParent();
    const auto ok_bb = llvm::BasicBlock::Create(*ctx->Context, "conv.float.ok" + uid, fn);
    const auto bad_bb = llvm::BasicBlock::Create(*ctx->Context, "conv.float.bad" + uid, fn);
    ctx->Builder.CreateCondBr(ctx->Builder.CreateAnd(lo_ok, hi_ok, "conv.float.in_range" + uid), ok_bb, bad_bb);

    // Reported by the type it was converting to, as the crash
    // path cannot format a float.
    ctx->Builder.SetInsertPoint(bad_bb);
    EmitCheckFailed(
      ctx, RuntimeCheck::FloatToInt,
      llvm::ConstantInt::get(i64_ty, static_cast<std::uint64_t>(width)),
      llvm::ConstantInt::get(i64_ty, is_signed ? 1 : 0));
    ctx->Builder.SetInsertPoint(ok_bb);
  }

  /// [CHECKED]
  /// Emit a shift whose result is defined for any distance
  /// (including one at or past the operand's width). A bare
  /// shl/lhsr poisons if the distance reaches the operand's
  /// bit width. Clamp the distance before the shift as well
  /// as afterwards.
  auto EmitDefinedShift(
    LlvmCtx *ctx, const fn_impls::BinOp op,
    llvm::Value *a, llvm::Value *b) -> llvm::Value* {
    // Create some predefined types that will be reused through
    // this function. Define the width and max for the shifts.
    const auto uid = "." + spp::utils::Uid();
    const auto ty = a->getType();
    const auto width = llvm::ConstantInt::get(ty, ty->getIntegerBitWidth());
    const auto max = llvm::ConstantInt::get(ty, ty->getIntegerBitWidth() - 1);

    // Do a width check and select the safe value if it trips;
    // this is the "before" check.
    const auto too_wide = ctx->Builder.CreateICmpUGE(b, width, "shift.wide" + uid);
    const auto safe = ctx->Builder.CreateSelect(too_wide, max, b, "shift.safe" + uid);
    const auto raw = op == fn_impls::BinOp::Shl
      ? ctx->Builder.CreateShl(a, safe, "shift.raw" + uid)
      : ctx->Builder.CreateLShr(a, safe, "shift.raw" + uid);

    // Do a width check afterwards too on the result. This is
    // the completion of the "<<" and ">>" safety.
    return ctx->Builder.CreateSelect(
      too_wide, llvm::ConstantInt::get(ty, 0), raw, "shift.result" + uid);
  }

  /// [CHECKED]
  /// Convert the intrinsic into a "llvm.<intrinsic>" (fully
  /// qualified), to get the overflow information too.
  auto OverflowIntrinsic(
    LlvmCtx const *ctx, const llvm::Intrinsic::IndependentIntrinsics intrinsic,
    llvm::Type *ty) -> llvm::Function* {
    // Operation name mapper based off of the intrinsic name. It
    // gives the final part of the fully qualified version.
    auto op = spp::StrView();
    switch (intrinsic) {
      case llvm::Intrinsic::sadd_with_overflow: op = "sadd";
        break;
      case llvm::Intrinsic::uadd_with_overflow: op = "uadd";
        break;
      case llvm::Intrinsic::ssub_with_overflow: op = "ssub";
        break;
      case llvm::Intrinsic::usub_with_overflow: op = "usub";
        break;
      case llvm::Intrinsic::smul_with_overflow: op = "smul";
        break;
      case llvm::Intrinsic::umul_with_overflow: op = "umul";
        break;
      default: spp::Ice("not a checked arithmetic intrinsic");
    }

    // Create the fully qualified name, and get its function.
    // Given it exists, return it.
    const auto name = std::format("llvm.{}.with.overflow.i{}", op, ty->getIntegerBitWidth());
    if (const auto declared = ctx->Module->getFunction(name); declared != nullptr) {
      return declared;
    }

    // Otherwise, create it with external linkage for a library
    // to provide implementation.
    const auto ret_ty = llvm::StructType::get(
      *ctx->Context, {ty, llvm::Type::getInt1Ty(*ctx->Context)});
    const auto fn_ty = llvm::FunctionType::get(
      ret_ty, {ty, ty}, false);
    return llvm::Function::Create(
      fn_ty, llvm::Function::ExternalLinkage, name, ctx->Module.get());
  }

  /// [CHECKED]
  /// The lowest level part of an arithmetic operation, with
  /// additional information for if it overflowed or not. This
  /// flows up to all math calling functions.
  auto EmitOverflowPair(
    LlvmCtx *ctx, const fn_impls::BinOp op,
    llvm::Value *a, llvm::Value *b)
    -> spp::Pair<llvm::Value*, llvm::Value*> {
    using BinOp = fn_impls::BinOp;
    const auto uid = "." + spp::utils::Uid();

    // Get the intrinsic based off the selected operation from
    // the calling function.
    auto intrinsic = llvm::Intrinsic::sadd_with_overflow;
    switch (op) {
      case BinOp::SAddChecked: intrinsic = llvm::Intrinsic::sadd_with_overflow;
        break;
      case BinOp::UAddChecked: intrinsic = llvm::Intrinsic::uadd_with_overflow;
        break;
      case BinOp::SSubChecked: intrinsic = llvm::Intrinsic::ssub_with_overflow;
        break;
      case BinOp::USubChecked: intrinsic = llvm::Intrinsic::usub_with_overflow;
        break;
      case BinOp::SMulChecked: intrinsic = llvm::Intrinsic::smul_with_overflow;
        break;
      case BinOp::UMulChecked: intrinsic = llvm::Intrinsic::umul_with_overflow;
        break;
      default: spp::Ice(std::format("not a checked arithmetic BinOp: {}", static_cast<int>(op)));
    }

    // Create a call ingo the intrinsic with the detected
    // types. Because of the S++ type system, the left and
    // right operands will be the same type.
    const auto pair = ctx->Builder.CreateCall(
      OverflowIntrinsic(ctx, intrinsic, a->getType()),
      {a, b}, "arith.checked" + uid);

    // Return a pair of function calls to get the value
    // and if there is an overflow or not.
    return {
      ctx->Builder.CreateExtractValue(pair, 0, "arith.value" + uid),
      ctx->Builder.CreateExtractValue(pair, 1, "arith.overflowed" + uid)
    };
  }

  /// [CHECKED]
  /// All math operations in S++ are checked, in debug and
  /// release mode; no wrapping or no undefined behaviour. This
  /// is to enforce maximum safety.
  auto EmitCheckedArith(
    LlvmCtx *ctx, const fn_impls::BinOp op,
    llvm::Value *a, llvm::Value *b) -> llvm::Value* {
    using BinOp = fn_impls::BinOp;
    const auto uid = "." + spp::utils::Uid();

    // One check per operation, so the report names the check
    // that failed rather than just "arithmetic". Distinct
    // checks cost one extra landing block per operation kind
    // per function, which is why they are per operation and
    // not per operation *site*.
    auto check = RuntimeCheck{};
    switch (op) {
      case BinOp::SAddChecked:
        check = RuntimeCheck::SignedAdd;
        break;
      case BinOp::UAddChecked:
        check = RuntimeCheck::UnsignedAdd;
        break;
      case BinOp::SSubChecked:
        check = RuntimeCheck::SignedSub;
        break;
      case BinOp::USubChecked:
        check = RuntimeCheck::UnsignedSub;
        break;
      case BinOp::SMulChecked:
        check = RuntimeCheck::SignedMul;
        break;
      case BinOp::UMulChecked:
        check = RuntimeCheck::UnsignedMul;
        break;
      default:
        spp::Ice(std::format("not a checked arithmetic BinOp: {}", static_cast<int>(op)));
    }

    // Call the low level overflow-safe arithmetic implementation
    // function, and destructure the results. The "overflowed"
    // flag will be used to branch.
    const auto fn = ctx->Builder.GetInsertBlock()->getParent();
    const auto [value, overflowed] = EmitOverflowPair(ctx, op, a, b);

    // Define the pass and fail states of an arithmetic operation,
    // and branch based on the "overflowed" flag.
    const auto ok_bb = llvm::BasicBlock::Create(
      *ctx->Context, "arith.ok" + uid, fn);
    const auto fail_bb = llvm::BasicBlock::Create(
      *ctx->Context, "arith.fail" + uid, fn);
    ctx->Builder.CreateCondBr(overflowed, fail_bb, ok_bb);

    // No branch weights: the call is "cold noreturn" and the block
    // ends in "unreachable", which is already everything block
    // placement needs to sink it.
    ctx->Builder.SetInsertPoint(fail_bb);
    EmitCheckFailed(ctx, check);
    ctx->Builder.SetInsertPoint(ok_bb);
    return value;
  }
}

auto spp::codegen::fn_impls::SimpleCreateFn(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ret_ty, Vec<llvm::Type*> const &param_tys)
  -> llvm::Function* {
  if (const auto declared = proto->GetLlvmFn(); declared != nullptr and declared->Target != nullptr) {
    return declared->Target;
  }

  const auto uid = "." + utils::Uid();
  const auto name = mangle::MangleFnName(*sm->CurrentScope, *proto);
  const auto fn_ty = llvm::FunctionType::get(ret_ty, param_tys, false);
  const auto fn = llvm::Function::Create(fn_ty, llvm::Function::ExternalLinkage, name, ctx->Module.get());
  const auto entry_bb = llvm::BasicBlock::Create(*ctx->Context, "entry" + uid, fn);
  ctx->Builder.SetInsertPoint(entry_bb);
  return fn;
}

// =========================================================================================================
// Layer 2: enum-driven operation dispatchers.
// =========================================================================================================

auto spp::codegen::fn_impls::IsShiftBinOp(
  const BinOp op) -> bool {
  switch (op) {
    case BinOp::Shl:
    case BinOp::LShr: return true;
    default: return false;
  }
}

auto spp::codegen::fn_impls::IsCompareBinOp(
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

auto spp::codegen::fn_impls::ApplyBinOp(
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
    case BinOp::SAddChecked:
    case BinOp::UAddChecked:
    case BinOp::SSubChecked:
    case BinOp::USubChecked:
    case BinOp::SMulChecked:
    case BinOp::UMulChecked: return EmitCheckedArith(ctx, op, a, b);
    default: Ice(std::format("Unsupported BinOp type: {}", name));
  }
}

auto spp::codegen::fn_impls::ApplyUnOp(
  LlvmCtx *ctx, const UnOp op, llvm::Value *a) -> llvm::Value* {
  const auto name = "result" + utils::Uid();
  switch (op) {
    case UnOp::Neg: return ctx->Builder.CreateNeg(a, name);
    case UnOp::Not: return ctx->Builder.CreateNot(a, name);
    case UnOp::FNeg: return ctx->Builder.CreateFNeg(a, name);
    default: Ice(std::format("Unsupported UnOp type: {}", name));
  }
}

auto spp::codegen::fn_impls::ApplyConvOp(
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
    default: Ice(std::format("Unsupported ConvOp type: {}", name));
  }
}

auto spp::codegen::fn_impls::SimpleIntrinsicBinop(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, const BinOp op) -> void {
  // "ty" (per the dispatcher) is always the function's declared RETURN type. For arithmetic ops that's also the
  // operand type ("T, T -> T"). For comparisons the return type is "Bool" (i1), so the *operand* type has to be read
  // off the function's own first parameter instead - "ty" alone can't give us both.
  const auto param0_name = proto->FnParamGroup->GetAllParams()[0]->ExtractName().get();
  const auto param0_type = sm->CurrentScope->FindVarSymbol(param0_name)->Type.get();
  const auto operand_ty = IsCompareBinOp(op)
    ? GetLlvmType(*sm->CurrentScope->FindTypeSymbol(param0_type), ctx)
    : ty;

  const auto fn = SimpleCreateFn(sm, proto, meta, ctx, ty, Vec{operand_ty, operand_ty});

  // A borrowed operand arrives as the address of the value rather than the value. Every comparison takes its two the
  // that way ("eq(this: &T, that: &T)"), where the arithmetic ones take theirs by value ("add(this: T, that: T)"), so
  // an operand is only usable as it arrives when its own parameter says it is - applying the operation to the two
  // addresses instead asks where the operands live rather than what they are, and for two distinct temporaries that
  // folds to a constant.
  const auto params = proto->FnParamGroup->GetAllParams();
  const auto operand_of = [&](llvm::Value *arg, asts::FunctionParameterAst const &param) {
    if (param.Type->GetConvention() == nullptr) { return arg; }
    const auto value_ty = GetLlvmTypeOf(
      analyse::scopes::TypeRef::Of(*param.Type, *sm->CurrentScope).WithoutConvention(), ctx);
    return llvm::cast<llvm::Value>(ctx->Builder.CreateLoad(value_ty, arg, "intrinsic.operand"));
  };

  const auto lhs = operand_of(fn->arg_begin(), *params[0]);
  auto rhs = operand_of(fn->arg_begin() + 1, *params[1]);

  // A shift is the one binary operation whose two operands are separately typed in the source ("bit_shl[T, U](this: T,
  // by: U)"), because a shift distance is a count rather than a value of the thing being shifted. Llvm requires both
  // operands of one, so the distance is widened or narrowed to the shifted value's type. Neither direction can lose a
  // meaningful distance: a distance that does not fit in "T" is already past the width being shifted.
  if (IsShiftBinOp(op) and rhs->getType() != lhs->getType()) {
    rhs = ctx->Builder.CreateZExtOrTrunc(rhs, lhs->getType(), "intrinsic.shift.by");
  }
  ctx->Builder.CreateRet(ApplyBinOp(ctx, op, lhs, rhs));
}

auto spp::codegen::fn_impls::SimpleIntrinsicBinopAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *, const BinOp op) -> void {
  // "(this: &mut T, that: U) -> Void": "ty" (per the dispatcher) is the declared return type "Void", not "T", so both
  // operand types are read off the parameters. The two are the same type for every operation but a shift, whose
  // distance is separately typed ("bit_shr_assign(&mut self, that: U32)") - so the slot being updated is sized from
  // "this" rather than from "that", or a "&mut U64" would be loaded and stored 32 bits at a time.
  const auto uid = "." + utils::Uid();
  const auto params = proto->FnParamGroup->GetAllParams();
  const auto value_ty = GetLlvmTypeOf(
    analyse::scopes::TypeRef::Of(*params[0]->Type, *sm->CurrentScope).WithoutConvention(), ctx);
  const auto operand_ty = GetLlvmTypeOf(
    analyse::scopes::TypeRef::Of(*params.back()->Type, *sm->CurrentScope).WithoutConvention(), ctx);

  const auto void_ty = llvm::Type::getVoidTy(*ctx->Context);
  const auto ptr_ty = llvm::cast<llvm::Type>(llvm::PointerType::get(*ctx->Context, 0));
  const auto fn = SimpleCreateFn(sm, proto, meta, ctx, void_ty, Vec{ptr_ty, operand_ty});

  const auto lhs = fn->arg_begin();
  auto rhs = llvm::cast<llvm::Value>(fn->arg_begin() + 1);
  const auto loaded_val = ctx->Builder.CreateLoad(value_ty, lhs, "intrinsic.assign.loaded" + uid);

  // As in the by-value form, a shift distance is widened or narrowed to the type being shifted, which llvm requires to
  // match. Neither direction loses a meaningful distance: one that does not fit in the value's type is already past
  // the width being shifted, and "ApplyBinOp" defines that case.
  if (IsShiftBinOp(op) and rhs->getType() != value_ty) {
    rhs = ctx->Builder.CreateZExtOrTrunc(rhs, value_ty, "intrinsic.shift.by" + uid);
  }
  const auto result = ApplyBinOp(ctx, op, loaded_val, rhs);
  ctx->Builder.CreateStore(result, lhs);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::fn_impls::SimpleIntrinsicUnop(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, const UnOp op) -> void {
  const auto fn = SimpleCreateFn(sm, proto, meta, ctx, ty, Vec{ty});
  const auto operand = fn->arg_begin();
  ctx->Builder.CreateRet(ApplyUnOp(ctx, op, operand));
}

auto spp::codegen::fn_impls::SimpleIntrinsicUnopAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *, const UnOp op)
  -> void {
  // "(this: &mut T) -> Void": "ty" (per the dispatcher) is the declared return type "Void", not "T" - the operand
  // type is read off "this" instead. "this" is itself a "&mut T" reference, but (matching how every other
  // reference-typed symbol in this file - e.g. "self" - is resolved) FindTypeSymbol/GetLlvmType already unwraps the
  // reference down to plain "T", not a raw pointer type.
  const auto uid = "." + utils::Uid();
  const auto this_param = proto->FnParamGroup->GetAllParams()[0];
  const auto operand_ty = GetLlvmTypeOf(
    analyse::scopes::TypeRef::Of(*this_param->Type, *sm->CurrentScope).WithoutConvention(), ctx);

  const auto void_ty = llvm::Type::getVoidTy(*ctx->Context);
  const auto ptr_ty = llvm::cast<llvm::Type>(llvm::PointerType::get(*ctx->Context, 0));
  const auto fn = SimpleCreateFn(sm, proto, meta, ctx, void_ty, Vec{ptr_ty});

  const auto lhs = fn->arg_begin();
  const auto loaded_val = ctx->Builder.CreateLoad(operand_ty, lhs, "intrinsic.assign.loaded" + uid);
  const auto result = ApplyUnOp(ctx, op, loaded_val);
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
    const spp::codegen::fn_impls::ConvOp op,
    llvm::Type const *src,
    llvm::Type const *dst)
    -> bool {
    using ConvOp = spp::codegen::fn_impls::ConvOp;
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
      default: spp::Ice(std::format("unhandled ConvOp: {}", static_cast<int>(op)));
    }
  }
}

auto spp::codegen::fn_impls::SimpleIntrinsicConv(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, const ConvOp op) -> void {
  // "ty" (per the dispatcher) is the function's declared RETURN type - the conversion's destination. The source
  // (operand) type is read off the function's own single parameter instead, since conversions genuinely go from one
  // type to a different one (e.g. "S32 -> F64"), unlike every other builder here where operand type == return type.
  const auto param = proto->FnParamGroup->GetAllParams()[0];
  const auto param_sym = sm->CurrentScope->FindVarSymbol(param->ExtractName().get());
  const auto src_ty = GetLlvmTypeOf(param_sym->TypeRefIn(*sm->CurrentScope).WithoutConvention(), ctx);
  const auto fn = SimpleCreateFn(sm, proto, meta, ctx, ty, Vec{src_ty});
  const auto operand = fn->arg_begin();

  // A conversion is only defined for some source/destination pairs - a truncation has to narrow, an extension has to
  // widen, a bit cast has to keep the size. An instantiation for a pair outside that is one nothing can call: the
  // conversions are selected by a "case w of { < that_w { utrunc } > that_w { uzext } else { bit_cast } }", and every
  // arm of that gets instantiated for the widths the enclosing instantiation binds, while only the arm the widths
  // choose can ever run. The other arms are given a body that says so, rather than an instruction llvm rejects.
  if (not ConvOpIsDefined(op, src_ty, ty)) {
    EmitCheckFailed(ctx, RuntimeCheck::Conversion);
    return;
  }
  if (op == ConvOp::FPToSI or op == ConvOp::FPToUI) {
    EmitFloatToIntCheck(ctx, operand, ty, op == ConvOp::FPToSI);
  }
  ctx->Builder.CreateRet(ApplyConvOp(ctx, op, operand, ty));
}

auto spp::codegen::fn_impls::SimpleIntrinsicIsConst(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, const bool is_float, const double value) -> void {
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto operand_ty = GetLlvmTypeOf(
    self_sym->TypeRefIn(*sm->CurrentScope).WithoutConvention(), ctx);
  const auto ptr_ty = llvm::cast<llvm::Type>(llvm::PointerType::get(*ctx->Context, 0));
  const auto fn = SimpleCreateFn(sm, proto, meta, ctx, ty, Vec{ptr_ty});
  const auto operand = ctx->Builder.CreateLoad(operand_ty, fn->arg_begin(), "intrinsic.operand" + uid);
  const auto name = "result" + utils::Uid();
  const auto result = is_float
    ? ctx->Builder.CreateFCmpOEQ(operand, llvm::ConstantFP::get(operand_ty, value), name)
    : ctx->Builder.CreateICmpEQ(operand, llvm::ConstantInt::get(operand_ty, static_cast<std::uint64_t>(value)), name);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::fn_impls::ApplyAtomicRmwOp(
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
    default: Ice(std::format("Unsupported AtomicRmwOp type: {}", static_cast<int>(op)));
  }
}

/**
 * Read the value a "cmp" generic parameter of the enclosing function was bound to on this instantiation, as an atomic
 * ordering. Llvm fixes the ordering of an atomic operation when the instruction is built - there is no atomic
 * instruction that takes a runtime ordering - so the orderings are generic parameters of the atomic intrinsics rather
 * than function parameters, and their values are read from the instantiation's symbol. Reading them off the
 * "llvm::Function"'s arguments cannot work: an "llvm::Argument" is never an "llvm::ConstantInt", whatever the caller
 * passed.
 * @param sm The scope manager, positioned on the instantiated function's scope.
 * @param name The name of the generic parameter holding the ordering.
 * @return The atomic ordering this instantiation was created for.
 */
static auto AtomicOrderingOf(
  ScopeManager *const sm, spp::Str const &name) -> llvm::AtomicOrdering {
  const auto param_name = IdentifierAst(0uz, name);
  const auto order_sym = sm->CurrentScope->FindVarSymbol(&param_name);
  SPP_ASSERT(order_sym != nullptr);

  // A template never reaches code generation, so the parameter is always bound to a value by the time this runs: read
  // it off the binding's identity.
  const auto order = U64Of(sm->CurrentScope->CompIdOfSymbol(*order_sym));
  SPP_ASSERT(order.has_value());
  return static_cast<llvm::AtomicOrdering>(*order);
}

auto spp::codegen::fn_impls::SimpleAtomicFetchRmw(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, const AtomicRmwOp op) -> void {
  // "(&self, val: T, order: U8) -> T": a plain method (not a coroutine, and not a free "_inner" function), so its
  // "llvm::Function" is already declared/opened by the time this runs, and "self" is
  // already bound; no "SimpleCreateFn"/env indirection needed.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);

  // "self" is "&Atom[T]" - a borrow, so its frame slot holds the *address* of the caller's "Atom[T]" instance, not
  // the instance itself; that address has to be loaded out before it can be used as a GEP base (see
  // "PostfixExpressionOperatorRuntimeMemberAccessAst::Stage11_CodeGen"'s "is_borrow" handling for the same rule).
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "atomic.fetch.self");
  const auto atom_ty = llvm::cast<llvm::StructType>(
    GetLlvmTypeOf(self_sym->TypeRefIn(*sm->CurrentScope).WithoutConvention(), ctx));
  const auto val_field_ptr = ctx->Builder.CreateStructGEP(atom_ty, self_ptr, 0, "atomic.fetch.val_ptr");
  const auto val_ty = atom_ty->getElementType(0);

  // "val" is the only non-"self" parameter; the ordering is a generic parameter.
  const auto val_param = proto->FnParamGroup->GetNonSelfParams()[0];
  const auto val_sym = sm->CurrentScope->FindVarSymbol(val_param->ExtractName().get());
  const auto val_arg = ctx->Builder.CreateLoad(val_ty, val_sym->LlvmInfo->Alloca, "atomic.fetch.operand");

  auto const &dl = ctx->Module->getDataLayout();
  const auto rmw_inst = ctx->Builder.CreateAtomicRMW(
    ApplyAtomicRmwOp(op), val_field_ptr, val_arg, dl.getABITypeAlign(val_ty),
    AtomicOrderingOf(sm, "order"));
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
   * as 127. The signedness is the type's own @c signed comp argument, so that is what is read; a type without one
   * (not an integer) is not signed.
   *
   * @param type The resolved return type of the intrinsic.
   * @return Whether it is a signed integer.
   */
  auto IsSignedIntegerType(
    spp::asts::TypeAst const &type,
    spp::analyse::scopes::Scope const &scope) -> bool {
    auto const *const sym = scope.FindTypeSymbol(&type);
    const auto signed_id = sym != nullptr ? sym->CompArgId("signed") : nullptr;
    auto const *const is_signed = signed_id != nullptr ? signed_id->AsBool() : nullptr;
    return is_signed != nullptr and *is_signed;
  }
}

auto spp::codegen::fn_impls::SimpleBinaryIntrinsicCall(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, const llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void {
  const auto uid = "." + utils::Uid();
  const auto fn = SimpleCreateFn(sm, proto, meta, ctx, ty, Vec{ty, ty});
  const auto lhs = fn->arg_begin();
  const auto rhs = fn->arg_begin() + 1;
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(ctx->Module.get(), intrinsic, {ty});
  const auto result = ctx->Builder.CreateCall(intrinsic_fn, {lhs, rhs}, "intrinsic.result" + uid);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::fn_impls::SimpleBinaryIntrinsicCallOverflow(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, const llvm::Intrinsic::IndependentIntrinsics intrinsic)
  -> void {
  // "ty" (per the dispatcher) is already the whole return type's own lowering - "(T, Bool)" is a literal struct, so
  // "ty" arrives as exactly "{T, i1}". "T" (the operand type "llvm.sadd.with.overflow" etc. actually take) is pulled
  // back out of that struct's first field, rather than needing a separate parameter lookup.
  const auto uid = "." + utils::Uid();
  const auto ret_ty = llvm::cast<llvm::StructType>(ty);
  const auto elem_ty = ret_ty->getElementType(0);
  const auto fn = SimpleCreateFn(sm, proto, meta, ctx, ret_ty, Vec{elem_ty, elem_ty});
  const auto lhs = fn->arg_begin();
  const auto rhs = fn->arg_begin() + 1;
  auto op = BinOp::SAddChecked;
  switch (intrinsic) {
    case llvm::Intrinsic::sadd_with_overflow: op = BinOp::SAddChecked;
      break;
    case llvm::Intrinsic::uadd_with_overflow: op = BinOp::UAddChecked;
      break;
    case llvm::Intrinsic::ssub_with_overflow: op = BinOp::SSubChecked;
      break;
    case llvm::Intrinsic::usub_with_overflow: op = BinOp::USubChecked;
      break;
    case llvm::Intrinsic::smul_with_overflow: op = BinOp::SMulChecked;
      break;
    case llvm::Intrinsic::umul_with_overflow: op = BinOp::UMulChecked;
      break;
    default: Ice("not a checked arithmetic intrinsic");
  }

  // "ret_ty" is the named struct every "(T, Bool)" tuple shares, so the two halves are packed into one of those
  // rather than returned as whatever anonymous pair they were computed as - llvm compares struct types by identity,
  // not by layout, so an alike-looking "{T, i1}" is still a different type.
  const auto [value, overflowed] = EmitOverflowPair(ctx, op, lhs, rhs);
  auto packed = llvm::cast<llvm::Value>(llvm::PoisonValue::get(ret_ty));
  packed = ctx->Builder.CreateInsertValue(packed, value, {0}, "intrinsic.packed" + uid);
  packed = ctx->Builder.CreateInsertValue(packed, overflowed, {1}, "intrinsic.packed" + uid);
  ctx->Builder.CreateRet(packed);
}

auto spp::codegen::fn_impls::SimpleUnaryIntrinsicCall(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, const llvm::Intrinsic::IndependentIntrinsics intrinsic) -> void {
  const auto uid = "." + utils::Uid();
  const auto fn = SimpleCreateFn(sm, proto, meta, ctx, ty, Vec{ty});
  const auto operand = fn->arg_begin();
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(ctx->Module.get(), intrinsic, {ty});
  const auto result = ctx->Builder.CreateCall(intrinsic_fn, {operand}, "intrinsic.result" + uid);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::fn_impls::SimpleGetValue(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty, llvm::Value *val) -> void {
  ignore = SimpleCreateFn(sm, proto, meta, ctx, ty, Vec{ty});
  ctx->Builder.CreateRet(val);
}

// =========================================================================================================
// Layer 2b: coroutine-specific shared helpers.
// =========================================================================================================

auto spp::codegen::fn_impls::SimpleCoroIter(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, const bool reverse, const bool borrow) -> void {
  // Implementation strategy for iterating an array - start at the
  // array pointer, and each step, increment the pointer value by
  // the array element size. At each position, load the value out
  // and place it into the yield slot, suspending after (mocks the
  // gen expression).
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "iter.self" + uid);
  const auto arr_ty = llvm::cast<llvm::ArrayType>(
    GetLlvmTypeOf(self_sym->TypeRefIn(*sm->CurrentScope).WithoutConvention(), ctx));
  const auto elem_ty = arr_ty->getElementType();
  const auto n = arr_ty->getNumElements();
  const auto i = MakeUnique<std::size_t>(not reverse ? n : 0);

  struct CustomExpr : asts::ExpressionAst {
    SPP_AST_KEY_FUNCTIONS_DEFAULT_IMPL
    SPP_AST_KIND(ExpressionAst)

    decltype(i) &I;
    decltype(arr_ty) &ArrTy;
    decltype(elem_ty) &ElemTy;
    decltype(self_ptr) &SelfPtr;
    decltype(borrow) &Borrow;

    CustomExpr(
      decltype(i) &i, decltype(arr_ty) &arr_ty, decltype(elem_ty) &elem_ty, decltype(self_ptr) &self_ptr,
      decltype(borrow) &borrow)
      : I(i), ArrTy(arr_ty), ElemTy(elem_ty), SelfPtr(self_ptr), Borrow(borrow) {}

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, asts::meta::CompilerMetaData *meta,
      LlvmCtx *ctx) -> llvm::Value* override {
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

auto spp::codegen::fn_impls::SimpleCoroViewIter(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, const bool reverse, const bool borrow) -> void {
  // A view is a "{data, length}" pair, and the length is a runtime value, so there is no count to unroll against the
  // way there is for an array. This emits the loop instead, with the suspend point inside the body: the counter is an
  // entry-block alloca, so the coroutine passes give it a frame slot and it holds its value across each suspend.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto i64_ty = llvm::Type::getInt64Ty(*ctx->Context);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "view.iter.self" + uid);

  const auto view_type_sym = self_sym->TypeRefIn(*sm->CurrentScope).Symbol;
  const auto view_llvm_type = llvm::cast<llvm::StructType>(GetLlvmType(*view_type_sym, ctx));

  const auto elem_ref = view_type_sym->TypeArgRef("T");
  const auto elem_llvm_type = elem_ref.Symbol != nullptr
    ? GetLlvmTypeOf(elem_ref, ctx)
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
    SPP_AST_KIND(ExpressionAst)

    decltype(index) &_Index;
    decltype(data) &_Data;
    decltype(uid) &_Uid;
    llvm::Type *_ElemTy;
    bool _Borrow;

    CustomExpr(
      decltype(index) &index, decltype(data) &data, decltype(uid) &uid, llvm::Type *elem_ty, const bool borrow)
      : _Index(index), _Data(data), _Uid(uid), _ElemTy(elem_ty), _Borrow(borrow) {}

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *, asts::meta::CompilerMetaData *,
      LlvmCtx *ctx) -> llvm::Value* override {
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

auto spp::codegen::fn_impls::SimpleCoroNonNullFwd(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx) -> void {
  // The NonNull[T] type can forward to &T/&mut T - modelled as a
  // pointer to the T type, stored within the NonNull[T]. Use two
  // loads, because there are two levels to go through: "NonNull[T]"
  // lowers to a pointer, but this takes it as "&self", so the
  // parameter is a pointer to *that*.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_slot = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.fwd.self");
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_slot, "non_null.fwd.data");

  struct CustomExpr : asts::ExpressionAst {
    SPP_AST_KEY_FUNCTIONS_DEFAULT_IMPL
    SPP_AST_KIND(ExpressionAst)

    decltype(self_ptr) &SelfPtr;

    explicit CustomExpr(
      decltype(self_ptr) &self_ptr)
      : SelfPtr(self_ptr) {}

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, asts::meta::CompilerMetaData *meta,
      LlvmCtx *ctx) -> llvm::Value* override {
      return SelfPtr;
    }
  };

  const auto mock_gen = MakeUnique<asts::GenExpressionAst>(
    nullptr, nullptr, MakeUnique<CustomExpr>(self_ptr));
  mock_gen->Stage11_CodeGen(sm, meta, ctx);
}

auto spp::codegen::fn_impls::SimpleCoroViewSlice(
  SPP_LLVM_FUNC_INFO,
  LlvmCtx *ctx)
  -> void {
  // To slice a view, we need to GEP in the "from" and
  // "upto" pointers, and return the memory between.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "view.self" + uid);

  // Extract the "from" and "upto" from the symbol's alloca storage in the symbol table. Taken from the parameters
  // without "self", because "self" is a parameter too - indexing the whole group reads "self" as "from" and "from" as
  // "upto", which slices from wherever the view happens to be stored.
  const auto value_params = proto->FnParamGroup->GetNonSelfParams();
  const auto from_param = value_params[0]->ExtractName();
  const auto upto_param = value_params[1]->ExtractName();

  const auto from_alloca = sm->CurrentScope->FindVarSymbol(from_param.get(), true)->LlvmInfo->Alloca;
  const auto upto_alloca = sm->CurrentScope->FindVarSymbol(upto_param.get(), true)->LlvmInfo->Alloca;

  const auto view_type_sym = self_sym->TypeRefIn(*sm->CurrentScope).Symbol;
  const auto view_llvm_type = llvm::cast<llvm::StructType>(GetLlvmType(*view_type_sym, ctx));

  const auto elem_ref = view_type_sym->TypeArgRef("T");
  const auto elem_llvm_type = elem_ref.Symbol != nullptr
    ? GetLlvmTypeOf(elem_ref, ctx)
    : llvm::Type::getInt8Ty(*ctx->Context);

  const auto data_idx = GetPhysicalFieldIndex(*view_type_sym->LlvmInfo, 0);
  const auto length_idx = GetPhysicalFieldIndex(*view_type_sym->LlvmInfo, 1);

  struct CustomExpr : asts::ExpressionAst {
    SPP_AST_KEY_FUNCTIONS_DEFAULT_IMPL
    SPP_AST_KIND(ExpressionAst)

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

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, asts::meta::CompilerMetaData *meta,
      LlvmCtx *ctx) -> llvm::Value* override {
      // Read the bounds out of the parameters' storage.
      const auto i64_ty = llvm::Type::getInt64Ty(*ctx->Context);
      const auto from_val = ctx->Builder.CreateLoad(i64_ty, _FromAlloca, "view.slice.from_val" + _Uid);
      const auto upto_val = ctx->Builder.CreateLoad(i64_ty, _UptoAlloca, "view.slice.upto_val" + _Uid);

      // The data the view spans, read out of its first field.
      const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
      const auto self_data_ptr = ctx->Builder.CreateStructGEP(
        _ViewTy, _SelfPtr, _DataIdx, "view.slice.self_data_ptr" + _Uid);
      const auto self_data = ctx->Builder.CreateLoad(ptr_ty, self_data_ptr, "view.slice.self_data" + _Uid);
      const auto self_length = ctx->Builder.CreateLoad(
        i64_ty, ctx->Builder.CreateStructGEP(_ViewTy, _SelfPtr, _LengthIdx, "view.slice.self_length_ptr" + _Uid),
        "view.slice.self_length" + _Uid);

      // Both bounds are checked before either is used: a slice
      // that ends before it starts would wrap its length, and
      // one ending past the view would span memory it does not
      // own. Each failure reports both numbers it compared.
      const auto fn = ctx->Builder.GetInsertBlock()->getParent();
      const auto order_ok_bb = llvm::BasicBlock::Create(*ctx->Context, "view.slice.order_ok" + _Uid, fn);
      const auto order_bad_bb = llvm::BasicBlock::Create(*ctx->Context, "view.slice.order_bad" + _Uid, fn);
      ctx->Builder.CreateCondBr(
        ctx->Builder.CreateICmpULE(from_val, upto_val, "view.slice.in_order" + _Uid), order_ok_bb, order_bad_bb);
      ctx->Builder.SetInsertPoint(order_bad_bb);
      EmitCheckFailed(ctx, RuntimeCheck::SliceOrder, from_val, upto_val);

      ctx->Builder.SetInsertPoint(order_ok_bb);
      const auto end_ok_bb = llvm::BasicBlock::Create(*ctx->Context, "view.slice.end_ok" + _Uid, fn);
      const auto end_bad_bb = llvm::BasicBlock::Create(*ctx->Context, "view.slice.end_bad" + _Uid, fn);
      ctx->Builder.CreateCondBr(
        ctx->Builder.CreateICmpULE(upto_val, self_length, "view.slice.end_in_bounds" + _Uid), end_ok_bb, end_bad_bb);
      ctx->Builder.SetInsertPoint(end_bad_bb);
      EmitCheckFailed(ctx, RuntimeCheck::SliceEnd, upto_val, self_length);
      ctx->Builder.SetInsertPoint(end_ok_bb);

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

auto spp::codegen::fn_impls::SimpleCoroContiguousFwd(
  SPP_LLVM_FUNC_INFO,
  LlvmCtx *const ctx,
  llvm::Value *const data,
  llvm::Value *const length)
  -> void {
  // The view type is taken from what the coroutine yields rather than rebuilt from the element type, because what is
  // written here has to be the same "View[T]" the caller resolved - a freshly built one has no symbol in this scope.
  const auto uid = "." + utils::Uid();
  auto *view_type_sym = analyse::scopes::TypeRef::Of(*proto->ReturnType, *sm->CurrentScope).Symbol;
  if (const auto yield = view_type_sym->TypeArgRef("Yield"); yield.Symbol != nullptr) { view_type_sym = yield.Symbol; }
  const auto view_llvm_type = llvm::cast<llvm::StructType>(GetLlvmType(*view_type_sym, ctx));
  const auto data_idx = GetPhysicalFieldIndex(*view_type_sym->LlvmInfo, 0);
  const auto length_idx = GetPhysicalFieldIndex(*view_type_sym->LlvmInfo, 1);

  struct CustomExpr final : asts::ExpressionAst {
    SPP_AST_KEY_FUNCTIONS_DEFAULT_IMPL
    SPP_AST_KIND(ExpressionAst)

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

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *, asts::meta::CompilerMetaData *,
      LlvmCtx *ctx) -> llvm::Value* override {
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

auto spp::codegen::fn_impls::SimpleCoroArrayFwd(
  SPP_LLVM_FUNC_INFO,
  LlvmCtx *const ctx)
  -> void {
  // "Arr[T, n]" lowers to an llvm "[n x T]" held inline, so the view over it is the array's own address paired with
  // the length the type itself carries. Nothing is read out of "self": the borrow it arrives as is already the
  // address of the first element.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "array.fwd.self" + uid);

  const auto arr_type_sym = self_sym->TypeRefIn(*sm->CurrentScope).Symbol;
  const auto arr_llvm_type = llvm::cast<llvm::ArrayType>(GetLlvmType(*arr_type_sym, ctx));

  const auto data = ctx->Builder.CreateConstInBoundsGEP2_64(
    arr_llvm_type, self_ptr, 0, 0, "array.fwd.data" + uid);
  const auto length = llvm::ConstantInt::get(
    llvm::Type::getInt64Ty(*ctx->Context), arr_llvm_type->getNumElements());
  SimpleCoroContiguousFwd(sm, proto, meta, ctx, data, length);
}

auto spp::codegen::fn_impls::SimpleCoroVectorFwd(
  SPP_LLVM_FUNC_INFO,
  LlvmCtx *const ctx)
  -> void {
  // A vector's elements live in its "buffer", a "RawBuf[T, A]" whose first attribute is the pointer to them. The live
  // region is "[0, length)": "start" only ever moves for the by-value move-iterator, which consumes the vector, so no
  // forwarded view can observe it non-zero.
  using analyse::utils::type_members::GetAllAttrs;
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto i64_ty = llvm::Type::getInt64Ty(*ctx->Context);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "vector.fwd.self" + uid);

  const auto vec_type_sym = self_sym->TypeRefIn(*sm->CurrentScope).Symbol;
  const auto vec_llvm_type = llvm::cast<llvm::StructType>(GetLlvmType(*vec_type_sym, ctx));
  const auto buffer_idx = GetPhysicalFieldIndex(*vec_type_sym->LlvmInfo, 0);
  const auto length_idx = GetPhysicalFieldIndex(*vec_type_sym->LlvmInfo, 1);

  // The buffer lays its own fields out independently of the vector's, so its pointer is reached through its own map
  // rather than assumed to have stayed first.
  const auto buffer_type_sym = spp::get<1>(GetAllAttrs(*vec_type_sym)[0]).Symbol;
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
  SimpleCoroContiguousFwd(sm, proto, meta, ctx, data, length);
}

auto spp::codegen::fn_impls::SimpleCoroViewIndex(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx) -> void {
  // To index a view, we need to GEP in the "from" and
  // "upto" pointers, and return the memory between.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "view.self" + uid);

  // Extract the "index" from the symbol's alloca storage in the symbol table. Taken from the parameters without
  // "self", because "self" is a parameter too - indexing the whole group reads "self" as the index, and steps into
  // the view by wherever it happens to be stored.
  const auto idx_param = proto->FnParamGroup->GetNonSelfParams()[0]->ExtractName();
  const auto idx_alloca = sm->CurrentScope->FindVarSymbol(idx_param.get(), true)->LlvmInfo->Alloca;

  // The element being indexed lives in the buffer the view spans, not in the view itself, so the view's own fields
  // have to be read to reach it: its data pointer to step from, its length to check against.
  const auto view_type_sym = self_sym->TypeRefIn(*sm->CurrentScope).Symbol;
  const auto view_llvm_type = llvm::cast<llvm::StructType>(GetLlvmType(*view_type_sym, ctx));

  const auto elem_ref = view_type_sym->TypeArgRef("T");
  const auto elem_llvm_type = elem_ref.Symbol != nullptr
    ? GetLlvmTypeOf(elem_ref, ctx)
    : llvm::Type::getInt8Ty(*ctx->Context);

  const auto data_idx = GetPhysicalFieldIndex(*view_type_sym->LlvmInfo, 0);
  const auto length_idx = GetPhysicalFieldIndex(*view_type_sym->LlvmInfo, 1);

  struct CustomExpr : asts::ExpressionAst {
    SPP_AST_KEY_FUNCTIONS_DEFAULT_IMPL
    SPP_AST_KIND(ExpressionAst)

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

    auto Stage11_CodeGen(analyse::scopes::ScopeManager *sm, asts::meta::CompilerMetaData *meta,
      LlvmCtx *ctx) -> llvm::Value* override {
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
      EmitCheckFailed(ctx, RuntimeCheck::Index, idx_val, self_length);

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
// Layer 3: BinOp (SimpleIntrinsicBinop)
// =========================================================================================================

auto spp::codegen::fn_impls::StdIntrinsicsSadd(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular add intrinsic (checked).
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::SAddChecked);
}

auto spp::codegen::fn_impls::StdIntrinsicsUadd(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular add intrinsic (checked).
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::UAddChecked);
}

auto spp::codegen::fn_impls::StdIntrinsicsSsub(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular sub intrinsic (checked).
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::SSubChecked);
}

auto spp::codegen::fn_impls::StdIntrinsicsUsub(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular sub intrinsic (checked).
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::USubChecked);
}

auto spp::codegen::fn_impls::StdIntrinsicsSmul(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular mul intrinsic (checked).
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::SMulChecked);
}

auto spp::codegen::fn_impls::StdIntrinsicsUmul(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular mul intrinsic (checked).
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::UMulChecked);
}

auto spp::codegen::fn_impls::StdIntrinsicsSdiv(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular dev intrinsic.
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::SDiv);
}

auto spp::codegen::fn_impls::StdIntrinsicsUdiv(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular dev intrinsic.
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::UDiv);
}

auto spp::codegen::fn_impls::StdIntrinsicsSrem(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular rem intrinsic.
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::SRem);
}

auto spp::codegen::fn_impls::StdIntrinsicsUrem(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular rem intrinsic.
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::URem);
}

auto spp::codegen::fn_impls::StdIntrinsicsBitShl(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::Shl);
}

auto spp::codegen::fn_impls::StdIntrinsicsBitShr(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::LShr);
}

auto spp::codegen::fn_impls::StdIntrinsicsBitIor(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::Or);
}

auto spp::codegen::fn_impls::StdIntrinsicsBitAnd(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::And);
}

auto spp::codegen::fn_impls::StdIntrinsicsBitXor(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::Xor);
}

auto spp::codegen::fn_impls::StdIntrinsicsEq(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::ICmpEQ);
}

auto spp::codegen::fn_impls::StdIntrinsicsOeq(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::FCmpOEQ);
}

auto spp::codegen::fn_impls::StdIntrinsicsNe(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::ICmpNE);
}

auto spp::codegen::fn_impls::StdIntrinsicsOne(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::FCmpONE);
}

auto spp::codegen::fn_impls::StdIntrinsicsSlt(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::ICmpSLT);
}

auto spp::codegen::fn_impls::StdIntrinsicsUlt(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::ICmpULT);
}

auto spp::codegen::fn_impls::StdIntrinsicsOlt(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::FCmpOLT);
}

auto spp::codegen::fn_impls::StdIntrinsicsSle(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::ICmpSLE);
}

auto spp::codegen::fn_impls::StdIntrinsicsUle(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::ICmpULE);
}

auto spp::codegen::fn_impls::StdIntrinsicsOle(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::FCmpOLE);
}

auto spp::codegen::fn_impls::StdIntrinsicsSgt(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::ICmpSGT);
}

auto spp::codegen::fn_impls::StdIntrinsicsUgt(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::ICmpUGT);
}

auto spp::codegen::fn_impls::StdIntrinsicsOgt(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::FCmpOGT);
}

auto spp::codegen::fn_impls::StdIntrinsicsSge(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::ICmpSGE);
}

auto spp::codegen::fn_impls::StdIntrinsicsUge(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::ICmpUGE);
}

auto spp::codegen::fn_impls::StdIntrinsicsOge(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::FCmpOGE);
}

auto spp::codegen::fn_impls::StdIntrinsicsFadd(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::FAdd);
}

auto spp::codegen::fn_impls::StdIntrinsicsFsub(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::FSub);
}

auto spp::codegen::fn_impls::StdIntrinsicsFmul(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::FMul);
}

auto spp::codegen::fn_impls::StdIntrinsicsFdiv(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::FDiv);
}

auto spp::codegen::fn_impls::StdIntrinsicsFrem(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::FRem);
}

auto spp::codegen::fn_impls::StdIntrinsicsSaddWrapping(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Wrapping add intrinsic.
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::Add);
}

auto spp::codegen::fn_impls::StdIntrinsicsUaddWrapping(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Wrapping add intrinsic.
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::Add);
}

auto spp::codegen::fn_impls::StdIntrinsicsSsubWrapping(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Wrapping sub intrinsic.
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::Sub);
}

auto spp::codegen::fn_impls::StdIntrinsicsUsubWrapping(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Wrapping sub intrinsic.
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::Sub);
}

auto spp::codegen::fn_impls::StdIntrinsicsSmulWrapping(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Wrapping mul intrinsic.
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::Mul);
}

auto spp::codegen::fn_impls::StdIntrinsicsUmulWrapping(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Wrapping mul intrinsic.
  SimpleIntrinsicBinop(sm, proto, meta, ctx, ty, BinOp::Mul);
}

// =========================================================================================================
// Layer 3: BinOp (SimpleIntrinsicBinopAssign)
// =========================================================================================================

auto spp::codegen::fn_impls::StdIntrinsicsSaddAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular add intrinsic (checked).
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::SAddChecked);
}

auto spp::codegen::fn_impls::StdIntrinsicsUaddAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular add intrinsic (checked).
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::UAddChecked);
}

auto spp::codegen::fn_impls::StdIntrinsicsSsubAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular sub intrinsic (checked).
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::SSubChecked);
}

auto spp::codegen::fn_impls::StdIntrinsicsUsubAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular sub intrinsic (checked).
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::USubChecked);
}

auto spp::codegen::fn_impls::StdIntrinsicsSmulAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular mul intrinsic (checked).
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::SMulChecked);
}

auto spp::codegen::fn_impls::StdIntrinsicsUmulAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular mul intrinsic (checked).
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::UMulChecked);
}

auto spp::codegen::fn_impls::StdIntrinsicsSdivAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular div intrinsic.
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::SDiv);
}

auto spp::codegen::fn_impls::StdIntrinsicsUdivAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular div intrinsic.
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::UDiv);
}

auto spp::codegen::fn_impls::StdIntrinsicsSremAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular rem intrinsic.
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::SRem);
}

auto spp::codegen::fn_impls::StdIntrinsicsUremAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Regular rem intrinsic.
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::URem);
}

auto spp::codegen::fn_impls::StdIntrinsicsBitShlAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::Shl);
}

auto spp::codegen::fn_impls::StdIntrinsicsBitShrAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::LShr);
}

auto spp::codegen::fn_impls::StdIntrinsicsBitIorAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::Or);
}

auto spp::codegen::fn_impls::StdIntrinsicsBitAndAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::And);
}

auto spp::codegen::fn_impls::StdIntrinsicsBitXorAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::Xor);
}

auto spp::codegen::fn_impls::StdIntrinsicsFaddAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::FAdd);
}

auto spp::codegen::fn_impls::StdIntrinsicsFsubAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::FSub);
}

auto spp::codegen::fn_impls::StdIntrinsicsFmulAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::FMul);
}

auto spp::codegen::fn_impls::StdIntrinsicsFdivAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::FDiv);
}

auto spp::codegen::fn_impls::StdIntrinsicsFremAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicBinopAssign(sm, proto, meta, ctx, ty, BinOp::FRem);
}

// =========================================================================================================
// Layer 3: UnOp (SimpleIntrinsicUnop / SimpleIntrinsicUnopAssign)
// =========================================================================================================

auto spp::codegen::fn_impls::StdIntrinsicsSneg(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicUnop(sm, proto, meta, ctx, ty, UnOp::Neg);
}

auto spp::codegen::fn_impls::StdIntrinsicsFneg(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicUnop(sm, proto, meta, ctx, ty, UnOp::FNeg);
}

auto spp::codegen::fn_impls::StdIntrinsicsBitNot(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicUnop(sm, proto, meta, ctx, ty, UnOp::Not);
}

auto spp::codegen::fn_impls::StdIntrinsicsBitNotAssign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicUnopAssign(sm, proto, meta, ctx, ty, UnOp::Not);
}

// =========================================================================================================
// Layer 3: ConvOp (SimpleIntrinsicConv)
// =========================================================================================================

auto spp::codegen::fn_impls::StdIntrinsicsSitofp(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicConv(sm, proto, meta, ctx, ty, ConvOp::SIToFP);
}

auto spp::codegen::fn_impls::StdIntrinsicsUitofp(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicConv(sm, proto, meta, ctx, ty, ConvOp::UIToFP);
}

auto spp::codegen::fn_impls::StdIntrinsicsFptrunc(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicConv(sm, proto, meta, ctx, ty, ConvOp::FPTrunc);
}

auto spp::codegen::fn_impls::StdIntrinsicsStrunc(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicConv(sm, proto, meta, ctx, ty, ConvOp::Trunc);
}

auto spp::codegen::fn_impls::StdIntrinsicsUtrunc(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicConv(sm, proto, meta, ctx, ty, ConvOp::Trunc);
}

auto spp::codegen::fn_impls::StdIntrinsicsSzext(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicConv(sm, proto, meta, ctx, ty, ConvOp::SExt);
}

auto spp::codegen::fn_impls::StdIntrinsicsUzext(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicConv(sm, proto, meta, ctx, ty, ConvOp::ZExt);
}

auto spp::codegen::fn_impls::StdIntrinsicsFpext(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicConv(sm, proto, meta, ctx, ty, ConvOp::FPExt);
}

auto spp::codegen::fn_impls::StdIntrinsicsBitCast(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicConv(sm, proto, meta, ctx, ty, ConvOp::BitCast);
}

auto spp::codegen::fn_impls::StdIntrinsicsFptosi(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicConv(sm, proto, meta, ctx, ty, ConvOp::FPToSI);
}

auto spp::codegen::fn_impls::StdIntrinsicsFptoui(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicConv(sm, proto, meta, ctx, ty, ConvOp::FPToUI);
}

// =========================================================================================================
// Layer 3: "is this constant" (SimpleIntrinsicIsConst)
// =========================================================================================================

auto spp::codegen::fn_impls::StdNumFloatIsZero(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicIsConst(sm, proto, meta, ctx, ty, true, 0.0);
}

auto spp::codegen::fn_impls::StdNumFloatIsOne(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicIsConst(sm, proto, meta, ctx, ty, true, 1.0);
}

auto spp::codegen::fn_impls::StdNumIntIsZero(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicIsConst(sm, proto, meta, ctx, ty, false, 0.0);
}

auto spp::codegen::fn_impls::StdNumIntIsOne(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleIntrinsicIsConst(sm, proto, meta, ctx, ty, false, 1.0);
}

// =========================================================================================================
// Layer 3: fixed values (SimpleGetValue)
// =========================================================================================================

auto spp::codegen::fn_impls::StdArrayNew(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // The array starts out uninitialized rather than zero-filled; callers that need defined contents go through
  // "new_filled"/"fill", which "mem_set" over this value afterwards.
  const auto val = llvm::UndefValue::get(ty);
  SimpleGetValue(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::fn_impls::StdNumFloatNegOne(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantFP::get(ty, -1.0);
  SimpleGetValue(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::fn_impls::StdNumFloatZero(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantFP::get(ty, 0.0);
  SimpleGetValue(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::fn_impls::StdNumFloatOne(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantFP::get(ty, 1.0);
  SimpleGetValue(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::fn_impls::StdNumIntNegOne(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // All-ones bit pattern is "-1" in two's complement, for any width.
  const auto val = llvm::ConstantInt::get(ty, llvm::APInt::getAllOnes(ty->getIntegerBitWidth()));
  SimpleGetValue(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::fn_impls::StdNumIntZero(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantInt::get(ty, 0);
  SimpleGetValue(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::fn_impls::StdNumIntOne(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantInt::get(ty, 1);
  SimpleGetValue(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::fn_impls::StdNumIntTwo(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  const auto val = llvm::ConstantInt::get(ty, 2);
  SimpleGetValue(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::fn_impls::StdIntrinsicsMinVal(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // The lowest representable value for this sized-integer type. LLVM integer types carry no sign, so signedness is
  // read off the resolved "Self" return type's name ("S32" vs "U32") instead of "ty".
  const auto is_signed = IsSignedIntegerType(*proto->ReturnType, *sm->CurrentScope);
  const auto bit_width = ty->getIntegerBitWidth();
  const auto val = llvm::ConstantInt::get(
    ty, is_signed ? llvm::APInt::getSignedMinValue(bit_width) : llvm::APInt::getMinValue(bit_width));
  SimpleGetValue(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::fn_impls::StdIntrinsicsMaxVal(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // The highest representable value for this sized-integer type. See StdIntrinsicsMinVal for why signedness
  // comes from the return type's name rather than "ty".
  const auto is_signed = IsSignedIntegerType(*proto->ReturnType, *sm->CurrentScope);
  const auto bit_width = ty->getIntegerBitWidth();
  const auto val = llvm::ConstantInt::get(
    ty, is_signed ? llvm::APInt::getSignedMaxValue(bit_width) : llvm::APInt::getMaxValue(bit_width));
  SimpleGetValue(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::fn_impls::StdIntrinsicsFminVal(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Most negative finite value representable by this float type.
  const auto val = llvm::ConstantFP::get(*ctx->Context, llvm::APFloat::getLargest(ty->getFltSemantics(), true));
  SimpleGetValue(sm, proto, meta, ctx, ty, val);
}

auto spp::codegen::fn_impls::StdIntrinsicsFmaxVal(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Largest finite value representable by this float type.
  const auto val = llvm::ConstantFP::get(*ctx->Context, llvm::APFloat::getLargest(ty->getFltSemantics(), false));
  SimpleGetValue(sm, proto, meta, ctx, ty, val);
}

// =========================================================================================================
// Layer 3: raw LLVM intrinsic calls, "(T, T) -> T" (SimpleBinaryIntrinsicCall)
// =========================================================================================================

auto spp::codegen::fn_impls::StdIntrinsicsSmax(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::smax);
}

auto spp::codegen::fn_impls::StdIntrinsicsUmax(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::umax);
}

auto spp::codegen::fn_impls::StdIntrinsicsSmin(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::smin);
}

auto spp::codegen::fn_impls::StdIntrinsicsUmin(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::umin);
}

auto spp::codegen::fn_impls::StdIntrinsicsFpowi(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // "float_powi(base: T, exponent: S32)" raises a float to
  // an *integer* power, so unlike every other binary intrinsic
  // its two operands are different types - and "llvm.powi"
  // is overloaded on both of them, not just the float.
  // Todo: Tidy this up
  const auto uid = "." + utils::Uid();
  const auto i32_ty = llvm::cast<llvm::Type>(llvm::Type::getInt32Ty(*ctx->Context));

  const auto fn = SimpleCreateFn(sm, proto, meta, ctx, ty, Vec{ty, i32_ty});
  const auto base = fn->arg_begin();
  const auto exponent = fn->arg_begin() + 1;
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(
    ctx->Module.get(), llvm::Intrinsic::powi, {ty, i32_ty});
  const auto result = ctx->Builder.CreateCall(intrinsic_fn, {base, exponent}, "intrinsic.result" + uid);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::fn_impls::StdIntrinsicsFpowf(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::pow);
}

auto spp::codegen::fn_impls::StdIntrinsicsFatan2(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::atan2);
}

auto spp::codegen::fn_impls::StdIntrinsicsFmax(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::maxnum);
}

auto spp::codegen::fn_impls::StdIntrinsicsFmin(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::minnum);
}

auto spp::codegen::fn_impls::StdIntrinsicsFcopysign(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::copysign);
}

auto spp::codegen::fn_impls::StdIntrinsicsSaddSaturating(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::sadd_sat);
}

auto spp::codegen::fn_impls::StdIntrinsicsUaddSaturating(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::uadd_sat);
}

auto spp::codegen::fn_impls::StdIntrinsicsSsubSaturating(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::ssub_sat);
}

auto spp::codegen::fn_impls::StdIntrinsicsUsubSaturating(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::usub_sat);
}

auto spp::codegen::fn_impls::StdIntrinsicsSshlSaturating(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::sshl_sat);
}

auto spp::codegen::fn_impls::StdIntrinsicsUshlSaturating(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::ushl_sat);
}

// =========================================================================================================
// Layer 3: raw LLVM intrinsic calls, "(T, T) -> (T, Bool)" (SimpleBinaryIntrinsicCallOverflow)
// =========================================================================================================

auto spp::codegen::fn_impls::StdIntrinsicsSaddOverflow(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCallOverflow(sm, proto, meta, ctx, ty, llvm::Intrinsic::sadd_with_overflow);
}

auto spp::codegen::fn_impls::StdIntrinsicsUaddOverflow(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCallOverflow(sm, proto, meta, ctx, ty, llvm::Intrinsic::uadd_with_overflow);
}

auto spp::codegen::fn_impls::StdIntrinsicsSsubOverflow(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCallOverflow(sm, proto, meta, ctx, ty, llvm::Intrinsic::ssub_with_overflow);
}

auto spp::codegen::fn_impls::StdIntrinsicsUsubOverflow(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCallOverflow(sm, proto, meta, ctx, ty, llvm::Intrinsic::usub_with_overflow);
}

auto spp::codegen::fn_impls::StdIntrinsicsSmulOverflow(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCallOverflow(sm, proto, meta, ctx, ty, llvm::Intrinsic::smul_with_overflow);
}

auto spp::codegen::fn_impls::StdIntrinsicsUmulOverflow(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleBinaryIntrinsicCallOverflow(sm, proto, meta, ctx, ty, llvm::Intrinsic::umul_with_overflow);
}

// =========================================================================================================
// Layer 3: raw LLVM intrinsic calls, "(T) -> T" (SimpleUnaryIntrinsicCall)
// =========================================================================================================

auto spp::codegen::fn_impls::StdIntrinsicsAbs(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::abs);
}

auto spp::codegen::fn_impls::StdIntrinsicsFsqrt(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::sqrt);
}

auto spp::codegen::fn_impls::StdIntrinsicsFsin(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::sin);
}

auto spp::codegen::fn_impls::StdIntrinsicsFcos(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::cos);
}

auto spp::codegen::fn_impls::StdIntrinsicsFtan(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::tan);
}

auto spp::codegen::fn_impls::StdIntrinsicsFasin(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::asin);
}

auto spp::codegen::fn_impls::StdIntrinsicsFacos(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::acos);
}

auto spp::codegen::fn_impls::StdIntrinsicsFatan(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::atan);
}

auto spp::codegen::fn_impls::StdIntrinsicsFsinh(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::sinh);
}

auto spp::codegen::fn_impls::StdIntrinsicsFcosh(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::cosh);
}

auto spp::codegen::fn_impls::StdIntrinsicsFtanh(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::tanh);
}

auto spp::codegen::fn_impls::StdIntrinsicsFexp(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::exp);
}

auto spp::codegen::fn_impls::StdIntrinsicsFexp2(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::exp2);
}

auto spp::codegen::fn_impls::StdIntrinsicsFexp10(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::exp10);
}

auto spp::codegen::fn_impls::StdIntrinsicsFlog(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::log);
}

auto spp::codegen::fn_impls::StdIntrinsicsFlog2(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::log2);
}

auto spp::codegen::fn_impls::StdIntrinsicsFlog10(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::log10);
}

auto spp::codegen::fn_impls::StdIntrinsicsFabs(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::fabs);
}

auto spp::codegen::fn_impls::StdIntrinsicsFfloor(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::floor);
}

auto spp::codegen::fn_impls::StdIntrinsicsFceil(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::ceil);
}

auto spp::codegen::fn_impls::StdIntrinsicsFtrunc(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::trunc);
}

auto spp::codegen::fn_impls::StdIntrinsicsFround(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::round);
}

auto spp::codegen::fn_impls::StdIntrinsicsBitreverse(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::bitreverse);
}

auto spp::codegen::fn_impls::StdIntrinsicsCtlz(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleUnaryIntrinsicCall(sm, proto, meta, ctx, ty, llvm::Intrinsic::ctlz);
}

auto spp::codegen::fn_impls::StdDebugBreakpointInternal(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  SimpleCreateFn(sm, proto, meta, ctx, ty, {});
  ctx->Builder.CreateIntrinsic(llvm::Intrinsic::debugtrap, {}, {}, {}, "");
  ctx->Builder.CreateRetVoid();
}

// =========================================================================================================
// Layer 3: three-way integer comparisons. "(this: &T, that: &T) -> S32": "ty" (per the dispatcher) is the return
// type "S32" - "T" is read off "this" instead. "llvm.scmp"/"llvm.ucmp" are overloaded on both the result type and
// the operand type, so both types are passed to "getOrInsertDeclaration".
// =========================================================================================================

auto spp::codegen::fn_impls::StdIntrinsicsScmp(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Both operands are declared "&T", so they arrive as pointers and the values have to be read out of them before
  // the comparison intrinsic - which takes the integers themselves - can be handed anything.
  const auto this_param = proto->FnParamGroup->GetAllParams()[0];
  const auto operand_ty = GetLlvmTypeOf(
    analyse::scopes::TypeRef::Of(*this_param->Type, *sm->CurrentScope).WithoutConvention(), ctx);

  const auto uid = "." + utils::Uid();
  const auto ptr_ty = llvm::cast<llvm::Type>(llvm::PointerType::get(*ctx->Context, 0));
  const auto fn = SimpleCreateFn(sm, proto, meta, ctx, ty, Vec{ptr_ty, ptr_ty});
  const auto lhs = ctx->Builder.CreateLoad(operand_ty, fn->arg_begin(), "intrinsic.lhs" + uid);
  const auto rhs = ctx->Builder.CreateLoad(operand_ty, fn->arg_begin() + 1, "intrinsic.rhs" + uid);
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(
    ctx->Module.get(), llvm::Intrinsic::scmp, {ty, operand_ty});
  const auto result = ctx->Builder.CreateCall(intrinsic_fn, {lhs, rhs}, "intrinsic.result" + uid);
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::fn_impls::StdIntrinsicsUcmp(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // Both operands are declared "&T", so they arrive as pointers and the values have to be read out of them before
  // the comparison intrinsic - which takes the integers themselves - can be handed anything.
  const auto this_param = proto->FnParamGroup->GetAllParams()[0];
  const auto operand_ty = GetLlvmTypeOf(
    analyse::scopes::TypeRef::Of(*this_param->Type, *sm->CurrentScope).WithoutConvention(), ctx);

  const auto uid = "." + utils::Uid();
  const auto ptr_ty = llvm::cast<llvm::Type>(llvm::PointerType::get(*ctx->Context, 0));
  const auto fn = SimpleCreateFn(sm, proto, meta, ctx, ty, Vec{ptr_ty, ptr_ty});
  const auto lhs = ctx->Builder.CreateLoad(operand_ty, fn->arg_begin(), "intrinsic.lhs" + uid);
  const auto rhs = ctx->Builder.CreateLoad(operand_ty, fn->arg_begin() + 1, "intrinsic.rhs" + uid);
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(
    ctx->Module.get(), llvm::Intrinsic::ucmp, {ty, operand_ty});
  const auto result = ctx->Builder.CreateCall(intrinsic_fn, {lhs, rhs}, "intrinsic.result" + uid);
  ctx->Builder.CreateRet(result);
}

// =========================================================================================================
// Layer 3: bespoke - "fpclass" needs two different argument types plus a Bool return, so it can't go through
// "SimpleBinaryIntrinsicCall" (which assumes both operands and the result share type "ty").
// =========================================================================================================

auto spp::codegen::fn_impls::StdIntrinsicsFpclass(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty)
  -> void {
  // "(value: T, flag: S32) -> Bool"; "ty" (per the dispatcher) is the return type "Bool" (i1) - "T" is read off the
  // "value" parameter instead.
  const auto value_param = proto->FnParamGroup->GetNonSelfParams()[0];
  const auto value_ty = GetLlvmTypeOf(
    analyse::scopes::TypeRef::Of(*value_param->Type, *sm->CurrentScope).WithoutConvention(), ctx);

  const auto uid = "." + utils::Uid();
  const auto i32_ty = llvm::cast<llvm::Type>(llvm::Type::getInt32Ty(*ctx->Context));
  const auto fn = SimpleCreateFn(sm, proto, meta, ctx, ty, Vec{value_ty, i32_ty});
  const auto value_arg = fn->arg_begin();
  const auto flag_arg = fn->arg_begin() + 1;
  const auto intrinsic_fn = llvm::Intrinsic::getOrInsertDeclaration(
    ctx->Module.get(), llvm::Intrinsic::is_fpclass, {value_ty});

  static constexpr auto kClassCount = 10u;
  const auto done_bb = llvm::BasicBlock::Create(*ctx->Context, "fpclass.done" + uid, fn);
  const auto other_bb = llvm::BasicBlock::Create(*ctx->Context, "fpclass.other" + uid, fn);
  const auto sw = ctx->Builder.CreateSwitch(flag_arg, other_bb, kClassCount);

  auto incoming = Vec<Pair<llvm::Value*, llvm::BasicBlock*>>();
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

auto spp::codegen::fn_impls::StdArrayIterMov(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleCoroIter(sm, proto, meta, ctx, false, false);
}

auto spp::codegen::fn_impls::StdArrayReverseIterMov(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleCoroIter(sm, proto, meta, ctx, true, false);
}

auto spp::codegen::fn_impls::StdArrayFwdRef(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  SimpleCoroArrayFwd(sm, proto, meta, ctx);
}

auto spp::codegen::fn_impls::StdArrayFwdMut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  SimpleCoroArrayFwd(sm, proto, meta, ctx);
}

auto spp::codegen::fn_impls::StdVectorFwdRef(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  SimpleCoroVectorFwd(sm, proto, meta, ctx);
}

auto spp::codegen::fn_impls::StdVectorFwdMut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  SimpleCoroVectorFwd(sm, proto, meta, ctx);
}

auto spp::codegen::fn_impls::StdGeneratorSend(
  SPP_LLVM_FUNC_INFO,
  LlvmCtx *ctx,
  llvm::Type *)
  -> void {
  // Dummy function for analysis. Still needs terminating. The ".res()" operator handles the lowering for generators
  // there, so reaching this body means "send" was called directly rather than through it.
  EmitCheckFailed(ctx, RuntimeCheck::GeneratorSend);
}

auto spp::codegen::fn_impls::StdGeneratorOnceSend(
  SPP_LLVM_FUNC_INFO,
  LlvmCtx *ctx,
  llvm::Type *)
  -> void {
  // Dummy function for analysis. Still needs terminating. The ".res()" operator handles the lowering for generators
  // there, so reaching this body means "send" was called directly rather than through it.
  EmitCheckFailed(ctx, RuntimeCheck::GeneratorSend);
}

auto spp::codegen::fn_impls::StdGeneratorDrop(
  SPP_LLVM_FUNC_INFO,
  LlvmCtx *ctx,
  llvm::Type *)
  -> void {
  //

  // A generator is a bare coroutine handle, and destroying one means destroying the frame it refers to - nothing else
  // frees that frame. "self" is taken by move, so its slot holds the handle itself, and that slot is the address the
  // destruction works through. "EmitDrop" is what knows to lower a generator to "llvm.coro.destroy", including the
  // null check for a handle that was never assigned one.
  const auto self_param = proto->FnParamGroup->GetSelfParam();
  const auto self_sym = sm->CurrentScope->FindVarSymbol(self_param->ExtractName().get());
  const auto self_ty_sym = sm->CurrentScope->FindSelfSymbol()->AsBound();
  EmitDrop(TypeRef::Of(*self_ty_sym, *sm->CurrentScope), self_sym->LlvmInfo->Alloca, sm, meta, ctx);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::fn_impls::StdStringViewSliceRef(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  SimpleCoroViewSlice(sm, proto, meta, ctx);
}

auto spp::codegen::fn_impls::StdStringViewSliceMut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  SimpleCoroViewSlice(sm, proto, meta, ctx);
}

auto spp::codegen::fn_impls::StdViewIndexRef(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  SimpleCoroViewIndex(sm, proto, meta, ctx);
}

auto spp::codegen::fn_impls::StdViewIndexMut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  SimpleCoroViewIndex(sm, proto, meta, ctx);
}

auto spp::codegen::fn_impls::StdViewSliceRef(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  SimpleCoroViewSlice(sm, proto, meta, ctx);
}

auto spp::codegen::fn_impls::StdViewSliceMut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  SimpleCoroViewSlice(sm, proto, meta, ctx);
}

auto spp::codegen::fn_impls::StdViewIterRef(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleCoroViewIter(sm, proto, meta, ctx, false, true);
}

auto spp::codegen::fn_impls::StdViewIterMut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleCoroViewIter(sm, proto, meta, ctx, false, true);
}

auto spp::codegen::fn_impls::StdViewReverseIterRef(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleCoroViewIter(sm, proto, meta, ctx, true, true);
}

auto spp::codegen::fn_impls::StdViewReverseIterMut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleCoroViewIter(sm, proto, meta, ctx, true, true);
}

auto spp::codegen::fn_impls::StdCffiCClosureFrom(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  // A closure and a "CClosure" are the same two pointers in the same order - the function and the environment it
  // captures - so this reads the pair back out under the other name. Nothing is copied, and nothing can be: the
  // environment's size is not in the closure's type, which is why it is heap allocated where it is built.
  const auto uid = "." + utils::Uid();
  const auto value_param = proto->FnParamGroup->GetNonSelfParams()[0];
  const auto value_sym = sm->CurrentScope->FindVarSymbol(value_param->ExtractName().get());

  // Both sides are two pointers, but they are different named struct types, and llvm holds a return to the exact one
  // the function declares - so the fields are read out of the closure and put back into "CClosure"'s own type.
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto pair_ty = llvm::StructType::get(*ctx->Context, {ptr_ty, ptr_ty});
  const auto pair = ctx->Builder.CreateLoad(pair_ty, value_sym->LlvmInfo->Alloca, "c_closure.from.pair" + uid);

  const auto ret_ty = ctx->Builder.GetInsertBlock()->getParent()->getReturnType();
  auto out = llvm::cast<llvm::Value>(llvm::PoisonValue::get(ret_ty));
  out = ctx->Builder.CreateInsertValue(
    out, ctx->Builder.CreateExtractValue(pair, {0}, "c_closure.from.fn" + uid), {0});
  out = ctx->Builder.CreateInsertValue(
    out, ctx->Builder.CreateExtractValue(pair, {1}, "c_closure.from.env" + uid), {1});
  ctx->Builder.CreateRet(out);
}

auto spp::codegen::fn_impls::StdFunctionFunMovDrop(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  // A closure that captures anything it can carry away holds its environment on the heap, because it may outlive the
  // frame that made it (see "ClosureExpressionAst::Stage11_CodeGen"). Dropping the closure is what releases it; the
  // environment's own header says whether there is storage to free ("EmitReleaseClosureEnv"), because a "FunRef"
  // dropped as a "FunMov" has its environment on a stack. The value is the "{fn, env}" pair, so the environment is
  // the second field.
  //
  // The captures inside it are not destroyed, only the storage holding them. Doing better needs the closure's type to
  // say what it captured, and it does not: every closure with the same signature has the same type, so there is no
  // per-closure destructor to reach from here. Todo: give a closure a type of its own, and drop its captures.
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto pair_ty = llvm::StructType::get(*ctx->Context, {ptr_ty, ptr_ty});

  const auto pair = ctx->Builder.CreateLoad(pair_ty, self_sym->LlvmInfo->Alloca, "fun_mov.drop.pair" + uid);
  const auto env = ctx->Builder.CreateExtractValue(pair, {1}, "fun_mov.drop.env" + uid);
  EmitReleaseClosureEnv(env, ctx);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::fn_impls::StdNonNullRead(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto data_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.read.data_ptr" + uid);

  // Add a Void guard to cover all eventualities of the generic
  // instantiation of intrinsic functions. Use the special return
  // void instruction in this case.
  if (IsValuelessType(ty)) {
    ctx->Builder.CreateRetVoid();
    return;
  }

  const auto val = ctx->Builder.CreateLoad(ty, data_ptr, "non_null.read.val" + uid);
  ctx->Builder.CreateRet(val);
}

auto spp::codegen::fn_impls::StdNonNullWrite(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.write.self");
  const auto data_ptr = ctx->Builder.CreateLoad(ptr_ty, self_ptr, "non_null.write.data_ptr");

  // Add a Void guard to cover all eventualities of the generic
  // instantiation of intrinsic functions. In this case, a Void
  // generic arg means the param is removed from the signature.
  // Use the special return void instruction in this case.
  const auto value_params = proto->FnParamGroup->GetNonSelfParams();
  if (value_params.empty()) {
    ctx->Builder.CreateRetVoid();
    return;
  }

  const auto value_param = value_params[0];
  const auto value_sym = sm->CurrentScope->FindVarSymbol(value_param->ExtractName().get());
  const auto value_ty = GetLlvmTypeOf(
    analyse::scopes::TypeRef::Of(*value_param->Type, *sm->CurrentScope).WithoutConvention(), ctx);
  const auto value_val = ctx->Builder.CreateLoad(value_ty, value_sym->LlvmInfo->Alloca, "non_null.write.value");

  ctx->Builder.CreateStore(value_val, data_ptr);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::fn_impls::StdNonNullRaw(
  SPP_LLVM_FUNC_INFO,
  LlvmCtx *ctx,
  llvm::Type *ty)
  -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.raw.self");
  const auto data_ptr = ctx->Builder.CreateLoad(ptr_ty, self_ptr, "non_null.raw.data_ptr");

  const auto ptr_struct_ty = llvm::cast<llvm::StructType>(ty);
  const auto addr_val = ctx->Builder.CreatePtrToInt(data_ptr, ptr_struct_ty->getElementType(0), "non_null.raw.addr");
  const auto poison = llvm::PoisonValue::get(ptr_struct_ty);
  const auto result = ctx->Builder.CreateInsertValue(poison, addr_val, {0}, "non_null.raw.result");
  ctx->Builder.CreateRet(result);
}

auto spp::codegen::fn_impls::StdNonNullEraseType(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.erase_type.self");
  ctx->Builder.CreateRet(self_ptr);
}

auto spp::codegen::fn_impls::StdNonNullCast(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "non_null.cast.self");
  ctx->Builder.CreateRet(self_ptr);
}

auto spp::codegen::fn_impls::StdNonNullFromPtrInner(
  SPP_LLVM_FUNC_INFO,
  LlvmCtx *ctx,
  llvm::Type *ty)
  -> void {
  //
  const auto ptr_param = proto->FnParamGroup->GetAllParams()[0];
  const auto ptr_sym = sm->CurrentScope->FindVarSymbol(ptr_param->ExtractName().get());
  const auto ptr_struct_ty = llvm::cast<llvm::StructType>(
    GetLlvmTypeOf(analyse::scopes::TypeRef::Of(*ptr_param->Type, *sm->CurrentScope).WithoutConvention(), ctx));
  const auto addr_field_ptr = ctx->Builder.CreateStructGEP(
    ptr_struct_ty, ptr_sym->LlvmInfo->Alloca, 0, "non_null.from_ptr.addr_field");
  const auto addr_val = ctx->Builder.CreateLoad(
    ptr_struct_ty->getElementType(0), addr_field_ptr, "non_null.from_ptr.addr");

  const auto data_ptr = ctx->Builder.CreateIntToPtr(addr_val, ty, "non_null.from_ptr.data_ptr");
  ctx->Builder.CreateRet(data_ptr);
}

auto spp::codegen::fn_impls::StdNonNullFwdMut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleCoroNonNullFwd(sm, proto, meta, ctx);
}

auto spp::codegen::fn_impls::StdNonNullFwdRef(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleCoroNonNullFwd(sm, proto, meta, ctx);
}

auto spp::codegen::fn_impls::StdVolRead(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;

  //
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(
    ptr_ty, self_sym->LlvmInfo->Alloca, "vol.read.self" + uid);
  const auto self_ty_sym = sm->CurrentScope->FindSelfSymbol()->AsBound();
  const auto self_ty = GetLlvmType(*self_ty_sym, ctx);

  // Get the "value" alloca and load the value from it. The
  // field is the first field on the "self" object.
  const auto llvm_val_field = ctx->Builder.CreateStructGEP(
    self_ty, self_ptr, 0, "vol.read.val_field" + uid);

  const auto llvm_val = ctx->Builder.CreateLoad(
    ty, llvm_val_field, true, "vol.read.val" + uid);
  ctx->Builder.CreateRet(llvm_val);
}

auto spp::codegen::fn_impls::StdVolWrite(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;

  //
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(
    ptr_ty, self_sym->LlvmInfo->Alloca, "vol.read.self" + uid);
  const auto self_ty_sym = sm->CurrentScope->FindSelfSymbol()->AsBound();
  const auto self_ty = GetLlvmType(*self_ty_sym, ctx);

  // Get the llvm representation of the value being written
  // to this volatile value.
  const auto new_param = proto->FnParamGroup->GetNonSelfParams()[0];
  const auto new_alloca = sm->CurrentScope->FindVarSymbol(new_param->ExtractName().get(), true)->LlvmInfo->Alloca;
  const auto new_type = GetLlvmType(*sm->CurrentScope->FindTypeSymbol(new_param->Type.get()), ctx);
  const auto new_val = ctx->Builder.CreateLoad(new_type, new_alloca, "vol.write.new_val" + uid);

  // Get the "value" alloca and store the value into it. The
  // field is the first field on the "self" object.
  const auto llvm_val_field = ctx->Builder.CreateStructGEP(
    self_ty, self_ptr, 0, "vol.read.val_field" + uid);

  ctx->Builder.CreateStore(new_val, llvm_val_field, true);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::fn_impls::StdVolReplace(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "vol.replace.self" + uid);
  const auto slot_ty = llvm::cast<llvm::StructType>(
    GetLlvmTypeOf(self_sym->TypeRefIn(*sm->CurrentScope).WithoutConvention(), ctx));

  const auto val_field_ptr = ctx->Builder.CreateStructGEP(slot_ty, self_ptr, 0, "vol.replace.val_ptr" + uid);
  const auto val_ty = slot_ty->getElementType(0);

  const auto new_val_param = proto->FnParamGroup->GetNonSelfParams()[0];
  const auto new_val_sym = sm->CurrentScope->FindVarSymbol(new_val_param->ExtractName().get());
  const auto new_val_ptr = new_val_sym->LlvmInfo->Alloca;

  const auto old_val = ctx->Builder.CreateLoad(val_ty, val_field_ptr, true, "vol.replace.old" + uid);
  const auto new_val = ctx->Builder.CreateLoad(val_ty, new_val_ptr, true, "vol.replace.new" + uid);
  ctx->Builder.CreateStore(new_val, val_field_ptr, true);
  ctx->Builder.CreateRet(old_val);
}

auto spp::codegen::fn_impls::StdRawBufIndexRef(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleCoroViewIndex(sm, proto, meta, ctx);
}

auto spp::codegen::fn_impls::StdRawBufIndexMut(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleCoroViewIndex(sm, proto, meta, ctx);
}

auto spp::codegen::fn_impls::StdRawBufTakeAt(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  // Bounds-checked "take", which moves the element at an index
  // off the buffer and hands it back as "Some(val)", or "None"
  // when the index is past the buffer's capacity.
  using asts::generate::common_types_precompiled::SELF_VAR;

  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto self_ty_sym = sm->CurrentScope->FindSelfSymbol()->AsBound();
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "raw_buf.take_at.self" + uid);

  const auto elem_ref = self_ty_sym->TypeArgRef("T");
  const auto elem_ty_sym = elem_ref.Symbol;
  const auto elem_ty = GetLlvmTypeOf(elem_ref, ctx);

  // "self" is a parameter like any other, and is reached through
  // its own symbol above, so the declared parameters are counted
  // without it.
  const auto index_param = proto->FnParamGroup->GetNonSelfParams()[0];
  const auto index_sym = sm->CurrentScope->FindVarSymbol(index_param->ExtractName().get());
  const auto usize_ty = GetLlvmTypeOf(
    analyse::scopes::TypeRef::Of(*index_param->Type, *sm->CurrentScope).WithoutConvention(), ctx);
  const auto index_val = ctx->Builder.CreateLoad(
    usize_ty, index_sym->LlvmInfo->Alloca, "raw_buf.take_at.index" + uid);

  // The returned "Opt[T]", and the discriminants of its two
  // alternatives. They are asked for by name rather than
  // taken by position, because nothing about the variant
  // guarantees which order its members are declared in.
  const auto opt_ty_spp = proto->ReturnType->WithoutConvention();
  const auto opt_llvm_ty = GetLlvmTypeOf(analyse::scopes::TypeRef::Of(*opt_ty_spp, *sm->CurrentScope), ctx);
  const auto some_ty_spp = asts::generate::common_types::SomeType(
    proto->PosStart(), asts::AstCloneShared(elem_ty_sym->FqName()));
  const auto none_ty_spp = asts::generate::common_types::None(proto->PosStart());
  const auto opt_ref = analyse::scopes::TypeRef::Of(*opt_ty_spp, *sm->CurrentScope);
  const auto some_tag = GetVariantIndexOfMember(
    opt_ref, analyse::scopes::TypeRef::Of(*some_ty_spp, *sm->CurrentScope), *sm->CurrentScope);
  const auto none_tag = GetVariantIndexOfMember(
    opt_ref, analyse::scopes::TypeRef::Of(*none_ty_spp, *sm->CurrentScope), *sm->CurrentScope);
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

auto spp::codegen::fn_impls::StdRawBufPlaceAt(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  using asts::generate::common_types_precompiled::SELF_VAR;
  //
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto self_ty_sym = sm->CurrentScope->FindSelfSymbol()->AsBound();
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "vol.replace.self" + uid);

  const auto elem_ty = GetLlvmTypeOf(self_ty_sym->TypeArgRef("T"), ctx);

  // "self" is a parameter like any other, and is reached through its own symbol above, so the declared parameters
  // are counted without it.
  const auto index_param = proto->FnParamGroup->GetNonSelfParams()[0];
  const auto index_sym = sm->CurrentScope->FindVarSymbol(index_param->ExtractName().get());
  const auto usize_ty = GetLlvmTypeOf(
    analyse::scopes::TypeRef::Of(*index_param->Type, *sm->CurrentScope).WithoutConvention(), ctx);
  const auto index_val = ctx->Builder.CreateLoad(usize_ty, index_sym->LlvmInfo->Alloca, "raw_buf.place_at.index");

  const auto element_param = proto->FnParamGroup->GetNonSelfParams()[1];
  const auto element_sym = sm->CurrentScope->FindVarSymbol(element_param->ExtractName().get());
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

auto spp::codegen::fn_impls::StdRawBufShift(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto self_ty_sym = sm->CurrentScope->FindSelfSymbol()->AsBound();
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "vol.replace.self" + uid);

  const auto elem_ty = GetLlvmTypeOf(self_ty_sym->TypeArgRef("T"), ctx);

  const auto from_param = proto->FnParamGroup->GetNonSelfParams()[0];
  const auto upto_param = proto->FnParamGroup->GetNonSelfParams()[1];
  const auto count_param = proto->FnParamGroup->GetNonSelfParams()[2];
  const auto from_sym = sm->CurrentScope->FindVarSymbol(from_param->ExtractName().get());
  const auto upto_sym = sm->CurrentScope->FindVarSymbol(upto_param->ExtractName().get());
  const auto count_sym = sm->CurrentScope->FindVarSymbol(count_param->ExtractName().get());
  const auto usize_ty = GetLlvmTypeOf(
    analyse::scopes::TypeRef::Of(*from_param->Type, *sm->CurrentScope).WithoutConvention(), ctx);
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

auto spp::codegen::fn_impls::StdRawBufClearRange(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto uid = "." + utils::Uid();
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto self_ty_sym = sm->CurrentScope->FindSelfSymbol()->AsBound();
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "raw_buf.clear.self" + uid);

  const auto elem_ref = self_ty_sym->TypeArgRef("T");

  // An element that owns nothing has no destructor to run,
  // so the whole loop collapses to nothing rather than to
  // a loop with an empty body.
  if (not analyse::utils::drop_utils::NeedsDrop(elem_ref, *sm, meta)) {
    ctx->Builder.CreateRetVoid();
    return;
  }

  const auto elem_ty = GetLlvmTypeOf(elem_ref, ctx);
  const auto start_param = proto->FnParamGroup->GetNonSelfParams()[0];
  const auto count_param = proto->FnParamGroup->GetNonSelfParams()[1];
  const auto start_sym = sm->CurrentScope->FindVarSymbol(start_param->ExtractName().get());
  const auto count_sym = sm->CurrentScope->FindVarSymbol(count_param->ExtractName().get());
  const auto usize_ty = GetLlvmTypeOf(
    analyse::scopes::TypeRef::Of(*start_param->Type, *sm->CurrentScope).WithoutConvention(), ctx);
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
  EmitDrop(elem_ref, elem_addr, sm, meta, ctx);
  const auto next = ctx->Builder.CreateAdd(index, llvm::ConstantInt::get(usize_ty, 1), "raw_buf.clear.next" + uid);
  index->addIncoming(next, ctx->Builder.GetInsertBlock());
  ctx->Builder.CreateBr(cond_bb);

  ctx->Builder.SetInsertPoint(done_bb);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::fn_impls::StdMemOpsSizeOf(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  //
  const auto t_ast = asts::TypeIdentifierAst::FromString("T");
  const auto size_val = llvm::ConstantInt::get(ty, SizeOf(*sm, TypeRef::Of(*t_ast, *sm->CurrentScope)));
  ctx->Builder.CreateRet(size_val);
}

auto spp::codegen::fn_impls::StdMemOpsAlignOf(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  //
  const auto t_ast = asts::TypeIdentifierAst::FromString("T");
  const auto align_val = llvm::ConstantInt::get(ty, AlignOf(*sm, TypeRef::Of(*t_ast, *sm->CurrentScope)));
  ctx->Builder.CreateRet(align_val);
}

auto spp::codegen::fn_impls::StdMemOpsSizeOfVal(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  // Todo: this needs heap querying
  const auto value_param = proto->FnParamGroup->GetAllParams()[0];
  const auto value_sym = sm->CurrentScope->FindVarSymbol(value_param->ExtractName().get());
  const auto elem_ref = value_sym->TypeRefIn(*sm->CurrentScope).WithoutConvention();
  const auto size_val = llvm::ConstantInt::get(ty, SizeOf(*sm, elem_ref));
  ctx->Builder.CreateRet(size_val);
}

auto spp::codegen::fn_impls::StdMemOpsAlignOfVal(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  // Todo: this needs heap querying
  const auto value_param = proto->FnParamGroup->GetAllParams()[0];
  const auto value_sym = sm->CurrentScope->FindVarSymbol(value_param->ExtractName().get());
  const auto elem_ref = value_sym->TypeRefIn(*sm->CurrentScope).WithoutConvention();
  const auto align_val = llvm::ConstantInt::get(ty, AlignOf(*sm, elem_ref));
  ctx->Builder.CreateRet(align_val);
}

auto spp::codegen::fn_impls::StdMemOpsReplace(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  // "dest" is "&mut T" - a borrow, so its slot holds an address that must be loaded before use. "src" is a plain
  // by-value "T", so its own slot already holds it directly.
  const auto dest_param = proto->FnParamGroup->GetAllParams()[0];
  const auto dest_sym = sm->CurrentScope->FindVarSymbol(dest_param->ExtractName().get());
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto dest_ptr = ctx->Builder.CreateLoad(ptr_ty, dest_sym->LlvmInfo->Alloca, "mem.replace.dest_ptr");

  const auto src_param = proto->FnParamGroup->GetAllParams()[1];
  const auto src_sym = sm->CurrentScope->FindVarSymbol(src_param->ExtractName().get());

  const auto old_val = ctx->Builder.CreateLoad(ty, dest_ptr, "mem.replace.old");
  const auto new_val = ctx->Builder.CreateLoad(ty, src_sym->LlvmInfo->Alloca, "mem.replace.new");
  ctx->Builder.CreateStore(new_val, dest_ptr);
  ctx->Builder.CreateRet(old_val);
}

auto spp::codegen::fn_impls::StdMemOpsSwap(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  // "a" and "b" are both "&mut T", so each slot holds an address. Both values are read before either is written, so
  // the exchange needs no third value: no default is made to stand in, and nothing is dropped. The type handed in is
  // the return type's, "Void" here, so "T" is read off the parameter.
  const auto params = proto->FnParamGroup->GetAllParams();
  const auto ty = GetLlvmTypeOf(TypeRef::Of(*params[0]->Type->WithoutConvention(), *sm->CurrentScope), ctx);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto ptr_of = [&](auto const &param, StrView name) {
    const auto sym = sm->CurrentScope->FindVarSymbol(param->ExtractName().get());
    return ctx->Builder.CreateLoad(ptr_ty, sym->LlvmInfo->Alloca, name);
  };
  const auto a_ptr = ptr_of(params[0], "mem.swap.a_ptr");
  const auto b_ptr = ptr_of(params[1], "mem.swap.b_ptr");
  const auto a_val = ctx->Builder.CreateLoad(ty, a_ptr, "mem.swap.a");
  const auto b_val = ctx->Builder.CreateLoad(ty, b_ptr, "mem.swap.b");
  ctx->Builder.CreateStore(b_val, a_ptr);
  ctx->Builder.CreateStore(a_val, b_ptr);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::fn_impls::StdMemOpsDrop(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  // "val" is taken by move, so its slot holds the value itself rather than an address of one elsewhere, and that slot
  // is what the destruction works through. This is the owning counterpart of "drop_in_place": the value is consumed by
  // being passed in, so nothing is left behind in the caller for the destroyed storage to be read back out of.
  // A Void generic arg removes the parameter from the signature, and a Void has nothing to destroy.
  const auto params = proto->FnParamGroup->GetAllParams();
  if (params.empty()) {
    ctx->Builder.CreateRetVoid();
    return;
  }
  const auto val_param = params[0];
  const auto val_sym = sm->CurrentScope->FindVarSymbol(val_param->ExtractName().get());

  const auto t_ast = asts::TypeIdentifierAst::FromString("T");
  EmitDrop(TypeRef::Of(*t_ast, *sm->CurrentScope), val_sym->LlvmInfo->Alloca, sm, meta, ctx);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::fn_impls::StdMemOpsDropInPlace(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  // "ptr" is "&mut T" - a borrow, so its slot holds the
  // address of the value rather than the value (see
  // "StdMemOpsReplace" for the same shape). That
  // address is what the value is destroyed through.
  const auto ptr_param = proto->FnParamGroup->GetAllParams()[0];
  const auto ptr_sym = sm->CurrentScope->FindVarSymbol(ptr_param->ExtractName().get());
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto target_ptr = ctx->Builder.CreateLoad(ptr_ty, ptr_sym->LlvmInfo->Alloca, "drop_in_place.ptr");

  // The pointee type is the "T" this instantiation was
  // made for.
  const auto t_ast = asts::TypeIdentifierAst::FromString("T");
  EmitDrop(TypeRef::Of(*t_ast, *sm->CurrentScope), target_ptr, sm, meta, ctx);
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::fn_impls::StdThreadingAtomicIsLockFree(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  //

  const auto self_type_sym = sm->CurrentScope->FindSelfSymbol()->AsBound();
  const auto atom_ty = llvm::cast<llvm::StructType>(GetLlvmType(*self_type_sym, ctx));
  const auto val_ty = atom_ty->getElementType(0);

  //
  auto const &dl = ctx->Module->getDataLayout();
  const auto max_atomic_bits = dl.getLargestLegalIntTypeSizeInBits();
  const auto is_lock_free = val_ty->getIntegerBitWidth() <= max_atomic_bits;
  const auto bool_ty = llvm::Type::getInt1Ty(*ctx->Context);
  const auto val = llvm::ConstantInt::getBool(*ctx->Context, is_lock_free);
  SimpleGetValue(sm, proto, meta, ctx, bool_ty, val);
}

auto spp::codegen::fn_impls::StdThreadingAtomicFenceInner(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  // Create the fence function.
  const auto void_ty = llvm::Type::getVoidTy(*ctx->Context);
  SimpleCreateFn(sm, proto, meta, ctx, void_ty, Vec<llvm::Type*>{});

  // Build the function body.
  ctx->Builder.CreateFence(AtomicOrderingOf(sm, "order"));
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::fn_impls::StdThreadingAtomicLoadInner(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  //
  const auto uid = "." + utils::Uid();
  const auto ptr_ty = llvm::cast<llvm::Type>(llvm::PointerType::get(*ctx->Context, 0));
  const auto fn = SimpleCreateFn(sm, proto, meta, ctx, ty, Vec{ptr_ty});

  const auto ptr_arg = fn->arg_begin();

  const auto load_inst = ctx->Builder.CreateLoad(ty, ptr_arg, "atomic.load" + uid);
  load_inst->setAtomic(AtomicOrderingOf(sm, "order"));
  ctx->Builder.CreateRet(load_inst);
}

auto spp::codegen::fn_impls::StdThreadingAtomicStoreInner(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  //
  const auto val_param = proto->FnParamGroup->GetAllParams()[1];
  const auto val_ty = GetLlvmTypeOf(
    analyse::scopes::TypeRef::Of(*val_param->Type, *sm->CurrentScope).WithoutConvention(), ctx);

  const auto void_ty = llvm::Type::getVoidTy(*ctx->Context);
  const auto ptr_ty = llvm::cast<llvm::Type>(llvm::PointerType::get(*ctx->Context, 0));
  const auto fn = SimpleCreateFn(sm, proto, meta, ctx, void_ty, Vec{ptr_ty, val_ty});

  const auto ptr_arg = fn->arg_begin();
  const auto val_arg = fn->arg_begin() + 1;

  const auto store_inst = ctx->Builder.CreateStore(val_arg, ptr_arg);
  store_inst->setAtomic(AtomicOrderingOf(sm, "order"));
  ctx->Builder.CreateRetVoid();
}

auto spp::codegen::fn_impls::StdThreadingAtomicCompexInner(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  //
  const auto ret_ty = llvm::cast<llvm::StructType>(ty);
  const auto elem_ty = ret_ty->getElementType(0);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto fn = SimpleCreateFn(
    sm, proto, meta, ctx, ret_ty, Vec<llvm::Type*>{ptr_ty, elem_ty, elem_ty});

  const auto ptr_arg = fn->arg_begin();
  const auto old_arg = fn->arg_begin() + 1;
  const auto new_arg = fn->arg_begin() + 2;

  auto const &dl = ctx->Module->getDataLayout();
  const auto cmpxchg_inst = ctx->Builder.CreateAtomicCmpXchg(
    ptr_arg, old_arg, new_arg, dl.getABITypeAlign(elem_ty),
    AtomicOrderingOf(sm, "success_order"),
    AtomicOrderingOf(sm, "failure_order"));

  // Repack
  const auto uid = "." + utils::Uid();
  auto packed = llvm::cast<llvm::Value>(llvm::PoisonValue::get(ret_ty));
  packed = ctx->Builder.CreateInsertValue(
    packed, ctx->Builder.CreateExtractValue(cmpxchg_inst, {0}, "compex.value" + uid), {0}, "compex.packed" + uid);
  packed = ctx->Builder.CreateInsertValue(
    packed, ctx->Builder.CreateExtractValue(cmpxchg_inst, {1}, "compex.flag" + uid), {1}, "compex.packed" + uid);
  ctx->Builder.CreateRet(packed);
}

auto spp::codegen::fn_impls::StdThreadingAtomicCompexWeakInner(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *ty) -> void {
  //
  const auto ret_ty = llvm::cast<llvm::StructType>(ty);
  const auto elem_ty = ret_ty->getElementType(0);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto fn = SimpleCreateFn(
    sm, proto, meta, ctx, ret_ty, Vec<llvm::Type*>{ptr_ty, elem_ty, elem_ty});

  const auto ptr_arg = fn->arg_begin();
  const auto old_arg = fn->arg_begin() + 1;
  const auto new_arg = fn->arg_begin() + 2;

  auto const &dl = ctx->Module->getDataLayout();
  const auto cmpxchg_inst = ctx->Builder.CreateAtomicCmpXchg(
    ptr_arg, old_arg, new_arg, dl.getABITypeAlign(elem_ty),
    AtomicOrderingOf(sm, "success_order"),
    AtomicOrderingOf(sm, "failure_order"));
  cmpxchg_inst->setWeak(true);

  // Repack
  const auto uid = "." + utils::Uid();
  auto packed = llvm::cast<llvm::Value>(llvm::PoisonValue::get(ret_ty));
  packed = ctx->Builder.CreateInsertValue(
    packed, ctx->Builder.CreateExtractValue(cmpxchg_inst, {0}, "compex.value" + uid), {0}, "compex.packed" + uid);
  packed = ctx->Builder.CreateInsertValue(
    packed, ctx->Builder.CreateExtractValue(cmpxchg_inst, {1}, "compex.flag" + uid), {1}, "compex.packed" + uid);
  ctx->Builder.CreateRet(packed);
}

auto spp::codegen::fn_impls::StdThreadingAtomicFetchExchange(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleAtomicFetchRmw(sm, proto, meta, ctx, AtomicRmwOp::Xchg);
}

auto spp::codegen::fn_impls::StdThreadingAtomicFetchAnd(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleAtomicFetchRmw(sm, proto, meta, ctx, AtomicRmwOp::And);
}

auto spp::codegen::fn_impls::StdThreadingAtomicFetchNand(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleAtomicFetchRmw(sm, proto, meta, ctx, AtomicRmwOp::Nand);
}

auto spp::codegen::fn_impls::StdThreadingAtomicFetchOr(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleAtomicFetchRmw(sm, proto, meta, ctx, AtomicRmwOp::Or);
}

auto spp::codegen::fn_impls::StdThreadingAtomicFetchXor(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleAtomicFetchRmw(sm, proto, meta, ctx, AtomicRmwOp::Xor);
}

auto spp::codegen::fn_impls::StdThreadingAtomicFetchNot(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *) -> void {
  //
  using asts::generate::common_types_precompiled::SELF_VAR;
  const auto self_sym = sm->CurrentScope->FindVarSymbol(SELF_VAR.get(), true);
  const auto ptr_ty = llvm::PointerType::get(*ctx->Context, 0);
  const auto self_ptr = ctx->Builder.CreateLoad(ptr_ty, self_sym->LlvmInfo->Alloca, "atomic.fetch_not.self");

  const auto atom_ty = llvm::cast<llvm::StructType>(
    GetLlvmTypeOf(self_sym->TypeRefIn(*sm->CurrentScope).WithoutConvention(), ctx));
  const auto val_field_ptr = ctx->Builder.CreateStructGEP(atom_ty, self_ptr, 0, "atomic.fetch_not.val_ptr");
  const auto val_ty = atom_ty->getElementType(0);
  const auto val_arg = llvm::ConstantInt::getBool(*ctx->Context, true);

  auto const &dl = ctx->Module->getDataLayout();
  const auto rmw_inst = ctx->Builder.CreateAtomicRMW(
    ApplyAtomicRmwOp(AtomicRmwOp::Xor), val_field_ptr, val_arg, dl.getABITypeAlign(val_ty),
    AtomicOrderingOf(sm, "order"));
  ctx->Builder.CreateRet(rmw_inst);
}

auto spp::codegen::fn_impls::StdThreadingAtomicFetchAdd(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleAtomicFetchRmw(sm, proto, meta, ctx, AtomicRmwOp::Add);
}

auto spp::codegen::fn_impls::StdThreadingAtomicFetchSub(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleAtomicFetchRmw(sm, proto, meta, ctx, AtomicRmwOp::Sub);
}

auto spp::codegen::fn_impls::StdThreadingAtomicFetchFadd(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleAtomicFetchRmw(sm, proto, meta, ctx, AtomicRmwOp::FAdd);
}

auto spp::codegen::fn_impls::StdThreadingAtomicFetchFsub(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleAtomicFetchRmw(sm, proto, meta, ctx, AtomicRmwOp::FSub);
}

auto spp::codegen::fn_impls::StdThreadingAtomicFetchFmax(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleAtomicFetchRmw(sm, proto, meta, ctx, AtomicRmwOp::FMax);
}

auto spp::codegen::fn_impls::StdThreadingAtomicFetchFmin(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleAtomicFetchRmw(sm, proto, meta, ctx, AtomicRmwOp::FMin);
}

auto spp::codegen::fn_impls::StdThreadingAtomicFetchSmax(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleAtomicFetchRmw(sm, proto, meta, ctx, AtomicRmwOp::Max);
}

auto spp::codegen::fn_impls::StdThreadingAtomicFetchUmax(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleAtomicFetchRmw(sm, proto, meta, ctx, AtomicRmwOp::UMax);
}

auto spp::codegen::fn_impls::StdThreadingAtomicFetchSmin(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleAtomicFetchRmw(sm, proto, meta, ctx, AtomicRmwOp::Min);
}

auto spp::codegen::fn_impls::StdThreadingAtomicFetchUmin(
  SPP_LLVM_FUNC_INFO, LlvmCtx *ctx, llvm::Type *)
  -> void {
  SimpleAtomicFetchRmw(sm, proto, meta, ctx, AtomicRmwOp::UMin);
}

#pragma GCC diagnostic pop
