module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.function_implementation_lowered_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.builtins;
import spp.analyse.utils.type_utils;
import spp.asts.expression_ast;
import spp.asts.float_literal_ast;
import spp.asts.function_prototype_ast;
import spp.asts.integer_literal_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_func_impls;
import spp.codegen.llvm_type;
import spp.utils.traits;
import genex;
import std;
import numex.big_int;

SPP_MOD_BEGIN
auto spp::asts::FunctionImplementationLoweredAst::NewEmpty()
  -> Unique<FunctionImplementationLoweredAst> {
  // Empty AST.
  return MakeUnique<FunctionImplementationLoweredAst>(nullptr, decltype(Members)(), nullptr);
}

spp::asts::FunctionImplementationLoweredAst::~FunctionImplementationLoweredAst() = default;

auto spp::asts::FunctionImplementationLoweredAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  auto f = MakeUnique<FunctionImplementationLoweredAst>(
    AstClone(TokL),
    AstCloneVec(Members),
    AstClone(TokR));
  f->SetScopePtr(_ScopePtr);
  f->SetProtoPtr(_ProtoPtr);
  return f;
}

auto spp::asts::FunctionImplementationLoweredAst::SetProtoPtr(
  FunctionPrototypeAst *proto)
  -> void {
  // Non-owning: this is a back-pointer to the prototype that owns this "Impl", not something to clone/free.
  _ProtoPtr = proto;
}

auto spp::asts::FunctionImplementationLoweredAst::_ValidateZeroDivision(
  Vec<Unique<ExpressionAst>> const &args,
  ScopeManager const *sm) const
  -> void {
  //
  using analyse::errors::SppDivisionByZeroError;

  // The dividing builtins all take the divisor second.
  // Their "_assign" forms divide just the same.
  static const auto dividing_builtins = Vec<Str>{
    "std.intrinsics.sdiv", "std.intrinsics.sdiv_assign",
    "std.intrinsics.udiv", "std.intrinsics.udiv_assign",
    "std.intrinsics.srem", "std.intrinsics.srem_assign",
    "std.intrinsics.urem", "std.intrinsics.urem_assign",
    "std.intrinsics.float_div", "std.intrinsics.float_div_assign",
    "std.intrinsics.float_rem", "std.intrinsics.float_rem_assign",
  };
  if (not genex::contains(dividing_builtins, _ScopePtr) or args.Len() < 2) { return; }

  // The divisor has already been resolved to a literal,
  // so whether it is zero is known here.
  auto const &divisor = *args[1];
  const auto int_divisor = divisor.To<IntegerLiteralAst>();
  const auto flt_divisor = divisor.To<FloatLiteralAst>();
  const auto is_zero =
    (int_divisor != nullptr and int_divisor->BigVal() == 0) or
    (flt_divisor != nullptr and flt_divisor->BigVal() == 0);

  RaiseIf<SppDivisionByZeroError>(
    is_zero, {sm->CurrentScope}, ERR_ARGS(*this, divisor));
}

auto spp::asts::FunctionImplementationLoweredAst::_ValidateShiftAmount(
  Vec<Unique<ExpressionAst>> const &args,
  ScopeManager const *sm) const
  -> void {
  //
  using analyse::errors::SppShiftAmountOutOfBoundsError;

  // The shifting builtins take the amount second,
  // and shift the first operand's type.
  static const auto shifting_builtins = Vec<Str>{
    "std.intrinsics.bit_shl", "std.intrinsics.bit_shl_assign",
    "std.intrinsics.bit_shr", "std.intrinsics.bit_shr_assign",
  };
  if (not genex::contains(shifting_builtins, _ScopePtr) or args.Len() < 2) { return; }

  const auto value = args[0]->To<IntegerLiteralAst>();
  const auto amount = args[1]->To<IntegerLiteralAst>();
  if (value == nullptr or amount == nullptr) { return; }

  // The width is the digits in the type name ("s32" -> 32);
  // the pointer-sized names carry no digits and are however
  // wide a pointer is here.
  const auto digits = value->Type
    | genex::views::filter([](auto c) { return std::isdigit(static_cast<unsigned char>(c)); })
    | genex::to<Str>();
  const auto width = digits.empty() ? static_cast<std::int64_t>(sizeof(void*)) * 8 : std::stol(digits);

  RaiseIf<SppShiftAmountOutOfBoundsError>(
    amount->BigVal() >= numex::BigInt(width),
    {sm->CurrentScope}, ERR_ARGS(*this, *args[1], value->Type, width));
}

auto spp::asts::FunctionImplementationLoweredAst::Stage9_CompTimeResolve(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> void {
  if (analyse::utils::builtins::kBuiltinFuncs.at(_ScopePtr).cmp_fn == nullptr) {
    return;
  }

  auto &lowered_cmp_code = *analyse::utils::builtins::kBuiltinFuncs.at(_ScopePtr).cmp_fn;
  auto extracted_args = Vec<Unique<ExpressionAst>>{};
  for (auto &&[_, arg] : std::move(meta->CmpArgs)) {
    extracted_args.EmplaceBack(std::move(arg));
  }

  // A zero divisor has to be caught before the operation
  // runs. As this is the one place every comp-time builtin
  // is invoked from, all operand analysis must be fired
  // off from here.
  _ValidateZeroDivision(extracted_args, sm);
  _ValidateShiftAmount(extracted_args, sm);
  meta->CmpResult = lowered_cmp_code
    .preload_generics(sm, meta->CmpGnTypeArgs, meta->CmpGnCompArgs)
    .invoke(std::move(extracted_args));

  // analyse::errors::SemanticErrorBuilder<analyse::errors::SppInvalidComptimeOperationError>()
  //     .with_args(*this)
  //     .raises_from(sm->CurrentScope);
}

auto spp::asts::FunctionImplementationLoweredAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  // Use the builtin to build the llvm custom lowered code. The
  // lowering reads the prototype's own scope, so it runs before
  // the scope walk below moves the cursor off it.
  const auto ret_type = analyse::utils::type_utils::ResolveAndSubstituteSelfType(
    *_ProtoPtr->ReturnType, *sm->CurrentScope, *sm, *meta);
  analyse::utils::builtins::kBuiltinFuncs
    .at(_ScopePtr)
    .llvm_fn(sm, _ProtoPtr, meta, ctx, codegen::GetLlvmTypeOf(*ret_type, *sm->CurrentScope, ctx));

  // Skip scopes to get back to the parent scope (skipping inner
  // scopes on the lowered function - `!intrinsic` etc).
  const auto final_scope = sm->CurrentScope->FinalChildScope();
  while (sm->CurrentScope != final_scope) {
    sm->MoveToNextScope(false);
  }
  return nullptr;
}

auto spp::asts::FunctionImplementationLoweredAst::SetScopePtr(
  Str const &scope_str)
  -> void {
  // Set the scope string for this lowered function implementation.
  _ScopePtr = scope_str;
}

SPP_MOD_END
