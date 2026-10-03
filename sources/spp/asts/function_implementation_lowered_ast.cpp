module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.function_implementation_lowered_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.self_type;
import spp.analyse.utils.type_resolution;
import spp.asts.char_literal_ast;
import spp.asts.expression_ast;
import spp.asts.float_literal_ast;
import spp.asts.function_prototype_ast;
import spp.asts.integer_literal_ast;
import spp.asts.token_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.builtins;
import spp.codegen.llvm_fn_impls;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.strings;
import spp.utils.traits;
import genex;
import std;
import numex.big_int;

SPP_MOD_BEGIN
auto FunctionImplementationLoweredAst::NewEmpty() -> Unique<FunctionImplementationLoweredAst> {
  // Empty AST.
  return MakeUnique<FunctionImplementationLoweredAst>(nullptr, decltype(Members)(), nullptr);
}

FunctionImplementationLoweredAst::~FunctionImplementationLoweredAst() = default;

auto FunctionImplementationLoweredAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  auto f = MakeUnique<FunctionImplementationLoweredAst>(
    AstClone(TokL),
    AstCloneVec(Members),
    AstClone(TokR));
  f->SetScopePtr(_ScopePtr);
  f->SetProtoPtr(_ProtoPtr);
  return f;
}

auto FunctionImplementationLoweredAst::SetProtoPtr(
  FunctionPrototypeAst *proto) -> void {
  // Non-owning: this is a back-pointer to the prototype that
  // owns this "Impl", not something to clone/free.
  _ProtoPtr = proto;
}

auto FunctionImplementationLoweredAst::_ValidateZeroDivision(
  Vec<Unique<ExpressionAst>> const &args, ScopeManager const *sm, CompilerMetaData const *meta) const -> void {
  IMPORT_UTILS;

  // The dividing builtins all take the divisor second. Their
  // "_assign" forms divide just the same.
  static const auto dividing_builtins = Vec<Str>{
    "std.intrinsics.sdiv", "std.intrinsics.sdiv_assign",
    "std.intrinsics.udiv", "std.intrinsics.udiv_assign",
    "std.intrinsics.srem", "std.intrinsics.srem_assign",
    "std.intrinsics.urem", "std.intrinsics.urem_assign",
    "std.intrinsics.float_div", "std.intrinsics.float_div_assign",
    "std.intrinsics.float_rem", "std.intrinsics.float_rem_assign",
  };
  if (not genex::contains(dividing_builtins, _ScopePtr) or args.Len() < 2) { return; }

  // The divisor has already been resolved to a literal, so
  // whether it is zero is known here.
  auto const &divisor = *args[1];
  const auto int_divisor = divisor.To<IntegerLiteralAst>();
  const auto flt_divisor = divisor.To<FloatLiteralAst>();
  const auto is_zero =
    (int_divisor != nullptr and int_divisor->BigVal() == 0) or
    (flt_divisor != nullptr and flt_divisor->BigVal() == 0);

  // Folded from a call, the division is the call the user
  // wrote; neither this builtin nor its folded divisor is
  // written there.
  if (meta->CompTimeCallSite != nullptr) {
    RaiseIf<SppDivisionByZeroError>(
      is_zero, {meta->CompTimeCallSiteScope}, ERR_ARGS(*meta->CompTimeCallSite, *meta->CompTimeCallSite));
  }
  RaiseIf<SppDivisionByZeroError>(
    is_zero, {sm->CurrentScope}, ERR_ARGS(*this, divisor));
}

auto FunctionImplementationLoweredAst::_ValidateShiftAmount(
  Vec<Unique<ExpressionAst>> const &args, ScopeManager const *sm, CompilerMetaData const *meta) const -> void {
  IMPORT_UTILS;

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

  const auto too_wide = amount->BigVal() >= numex::BigInt(width);
  if (meta->CompTimeCallSite != nullptr) {
    RaiseIf<SppShiftAmountOutOfBoundsError>(
      too_wide, {meta->CompTimeCallSiteScope},
      ERR_ARGS(*meta->CompTimeCallSite, *meta->CompTimeCallSite, value->Type, width));
  }
  RaiseIf<SppShiftAmountOutOfBoundsError>(
    too_wide, {sm->CurrentScope}, ERR_ARGS(*this, *args[1], value->Type, width));
}

auto FunctionImplementationLoweredAst::Stage9_CompTimeResolve(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  if (codegen::builtins::kBuiltinFuncs.at(_ScopePtr).CompTimeImpl == nullptr) {
    return;
  }

  auto &lowered_cmp_code = *codegen::builtins::kBuiltinFuncs.at(_ScopePtr).CompTimeImpl;
  auto extracted_args = Vec<Unique<ExpressionAst>>{};
  for (auto &&[_, arg] : std::move(meta->CompTimeArgs)) {
    extracted_args.EmplaceBack(std::move(arg));
  }

  // A byte literal is a "U8", but it arrives as the char
  // literal it was written as, and every comp-time builtin
  // over "U8" takes integer literals - the cast to one threw
  // "std::bad_cast". It is read as the integer it stands for.
  for (auto &arg : extracted_args) {
    if (auto const *chr = arg->To<CharLiteralAst>(); chr != nullptr and chr->BytePrefix != nullptr) {
      const auto byte = spp::utils::strings::DecodeCharLiteral(chr->Val->TokenData) & 0xFFu;
      arg = MakeUnique<IntegerLiteralAst>(
        nullptr, MakeUnique<TokenAst>(0uz, lex::SppTokenType::LX_NUMBER, std::to_string(byte)), Str("u8"));
    }
  }

  // A zero divisor has to be caught before the operation
  // runs. As this is the one place every comp-time builtin
  // is invoked from, all operand analysis must be fired
  // off from here.
  _ValidateZeroDivision(extracted_args, sm, meta);
  _ValidateShiftAmount(extracted_args, sm, meta);
  meta->CompTimeResult = lowered_cmp_code
                    .PreloadGns(sm, meta->CompTimeGnTypeArgs, meta->CompTimeGnCompArgs)
                    .Invoke(std::move(extracted_args));

  // analyse::errors::SemanticErrorBuilder<analyse::errors::SppInvalidCompTimeOperationError>()
  //     .with_args(*this)
  //     .raises_from(sm->CurrentScope);
}

auto FunctionImplementationLoweredAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  IMPORT_UTILS;
  // Use the builtin to build the llvm custom lowered code. The
  // lowering reads the prototype's own scope, so it runs before
  // the scope walk below moves the cursor off it.
  const auto ret_type = self_type::SubstituteSelf(
        *_ProtoPtr->ReturnType, sm->CurrentScope->FindEnclosingSelfType(*meta).get(), sm, meta);

  codegen::builtins::kBuiltinFuncs
    .at(_ScopePtr)
    .LlvmImpl(sm, _ProtoPtr, meta, ctx, codegen::GetLlvmTypeOf(TypeRef::Of(*ret_type, *sm->CurrentScope), ctx));

  // Skip scopes to get back to the parent scope (skipping inner
  // scopes on the lowered function - `!intrinsic` etc).
  const auto final_scope = sm->CurrentScope->GetFinalChildScope();
  while (sm->CurrentScope != final_scope) {
    sm->MoveToNextScope(false);
  }
  return nullptr;
}

auto FunctionImplementationLoweredAst::SetScopePtr(
  Str const &scope_str) -> void {
  // Set the scope string for this lowered function implementation.
  _ScopePtr = scope_str;
}

SPP_MOD_END
