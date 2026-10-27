module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.function_implementation_lowered_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.comp_time_intrinsics;
import spp.analyse.utils.self_type;
import spp.analyse.utils.type_resolution;
import spp.asts.char_literal_ast;
import spp.asts.expression_ast;
import spp.asts.float_literal_ast;
import spp.asts.function_prototype_ast;
import spp.asts.identifier_ast;
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
  f->SetBuiltin(_Builtin);
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
  Vec<Unique<ExpressionAst>> const &args, CompilerMetaData const *meta) const -> void {
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
  if (not genex::contains(dividing_builtins, _Builtin->Name) or args.size() < 2) { return; }

  // The divisor has already been resolved to a literal, so
  // whether it is zero is known here.
  auto const &divisor = *args[1];
  const auto is_zero = comp_time_intrinsics::IsZeroDivisor(divisor);

  // Folded from a call, the division is the call the user
  // wrote; neither this builtin nor its folded divisor is
  // written there.
  if (meta->CompTimeCallSite != nullptr) {
    RaiseIf<SppDivisionByZeroError>(is_zero, ERR_ARGS(*meta->CompTimeCallSite, *meta->CompTimeCallSite));
  }
  RaiseIf<SppDivisionByZeroError>(is_zero, ERR_ARGS(*this, divisor));
}

auto FunctionImplementationLoweredAst::_ValidateShiftAmount(
  Vec<Unique<ExpressionAst>> const &args, CompilerMetaData const *meta) const -> void {
  IMPORT_UTILS;

  // The shifting builtins take the amount second,
  // and shift the first operand's type.
  static const auto shifting_builtins = Vec<Str>{
    "std.intrinsics.bit_shl", "std.intrinsics.bit_shl_assign",
    "std.intrinsics.bit_shr", "std.intrinsics.bit_shr_assign",
  };
  if (not genex::contains(shifting_builtins, _Builtin->Name) or args.size() < 2) { return; }

  const auto value = args[0]->To<IntegerLiteralAst>();
  const auto amount = args[1]->To<IntegerLiteralAst>();
  if (value == nullptr or amount == nullptr) { return; }

  const auto width = comp_time_intrinsics::IntegerWidth(value->TypeOrDefault());
  const auto too_wide = comp_time_intrinsics::IsShiftTooWide(*value, *amount);
  if (meta->CompTimeCallSite != nullptr) {
    RaiseIf<SppShiftAmountOutOfBoundsError>(
      too_wide,
      ERR_ARGS(*meta->CompTimeCallSite, *meta->CompTimeCallSite, value->Type, width));
  }
  RaiseIf<SppShiftAmountOutOfBoundsError>(too_wide, ERR_ARGS(*this, *args[1], value->Type, width));
}

auto FunctionImplementationLoweredAst::Stage11_CodeGen(
  ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* {
  IMPORT_UTILS;
  // Use the builtin to build the llvm custom lowered code. The
  // lowering reads the prototype's own scope, so it runs before
  // the scope walk below moves the cursor off it.
  const auto ret_type = self_type::SubstituteSelf(
    *_ProtoPtr->ReturnType, sm->CurrentScope->FindEnclosingSelfType(*meta).get(),
    *sm->CurrentScope, sm, meta);

  _Builtin->LlvmImpl(sm, _ProtoPtr, meta, ctx, codegen::GetLlvmTypeOf(TypeRef::Of(*ret_type, *sm->CurrentScope), ctx));

  // Skip scopes to get back to the parent scope (skipping inner
  // scopes on the lowered function - `!intrinsic` etc).
  ignore = sm->SkipSubtree(sm->CurrentScope);
  return nullptr;
}

auto FunctionImplementationLoweredAst::EvaluateCompTime(
  ScopeManager *sm, CompilerMetaData *meta) -> void {
  //
  if (_Builtin->CompTimeImpl == nullptr) {
    return;
  }

  auto &lowered_cmp_code = *_Builtin->CompTimeImpl;
  auto arg_names = Vec<Shared<IdentifierAst>>{};
  auto extracted_args = Vec<Unique<ExpressionAst>>{};
  for (auto &&[name, arg] : meta->CompTimeCall.FnArgs) {
    arg_names.emplace_back(name);
    extracted_args.emplace_back(std::move(arg));
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
  _ValidateZeroDivision(extracted_args, meta);
  _ValidateShiftAmount(extracted_args, meta);
  meta->CompTimeResult =
    lowered_cmp_code
    .PreloadGns(sm, meta->CompTimeCall.GnTypeArgs, meta->CompTimeCall.GnCompArgs)
    .Invoke(extracted_args);

  // An "_assign" builtin writes its "&mut" operand in place:
  // the arguments go back as they were left, for the call to
  // write back to the places they came from.
  for (auto i = 0uz; i < arg_names.size(); ++i) {
    meta->CompTimeCall.FnArgs[arg_names[i]] = std::move(extracted_args[i]);
  }

  // analyse::errors::SemanticErrorBuilder<analyse::errors::SppInvalidCompTimeOperationError>()
  //     .with_args(*this)
  //     .raises_from(sm->CurrentScope);
}

auto FunctionImplementationLoweredAst::SetBuiltin(
  codegen::builtins::LoweredFnImpl const *builtin) -> void {
  // Non-owning: an entry of the static "kBuiltinFuncs".
  _Builtin = builtin;
}

auto FunctionImplementationLoweredAst::Builtin() const
  -> codegen::builtins::LoweredFnImpl const* {
  return _Builtin;
}

SPP_MOD_END
