module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.float_literal_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.numbers;
import spp.utils.strings;
import spp.utils.types;
import llvm;

SPP_MOD_BEGIN
spp::asts::FloatLiteralAst::FloatLiteralAst(
  decltype(TokSign) &&tok_sign,
  decltype(IntVal) &&int_val,
  decltype(TokDot) &&tok_dot,
  decltype(FracVal) &&frac_val,
  Str &&type) :
  TokSign(std::move(tok_sign)),
  IntVal(std::move(int_val)),
  TokDot(std::move(tok_dot)),
  FracVal(std::move(frac_val)),
  Type(std::move(type)) {
}

spp::asts::FloatLiteralAst::~FloatLiteralAst() = default;

auto spp::asts::FloatLiteralAst::EqualsFloatLiteral(
  FloatLiteralAst const &other) const
  -> Ordering {
  // Equality based on the sign, integer part, fractional
  // part, and type postfix.
  if (
    ((not TokSign and not other.TokSign) or (TokSign and other.TokSign and *TokSign == *other.TokSign))
    and IntVal->TokenData == other.IntVal->TokenData
    and FracVal->TokenData == other.FracVal->TokenData
    and Type == other.Type) {
    return Ordering::equal;
  }
  return Ordering::less;
}

auto spp::asts::FloatLiteralAst::Equals(
  ExpressionAst const &other) const
  -> Ordering {
  // Reverse hook (double dispatch).
  return other.EqualsFloatLiteral(*this);
}

auto spp::asts::FloatLiteralAst::PosStart() const
  -> std::size_t {
  // Use the sign token or the integer part.
  return TokSign ? TokSign->PosStart() : IntVal->PosStart();
}

auto spp::asts::FloatLiteralAst::PosEnd() const
  -> std::size_t {
  // Use the fractional part.
  return FracVal->PosEnd();
}

auto spp::asts::FloatLiteralAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<FloatLiteralAst>(
    AstClone(TokSign),
    AstClone(IntVal),
    AstClone(TokDot),
    AstClone(FracVal),
    Type.c_str());
}

auto spp::asts::FloatLiteralAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokSign);
  SPP_STRING_APPEND(IntVal);
  SPP_STRING_APPEND(TokDot);
  SPP_STRING_APPEND(FracVal);
  raw_string.append("_").append(Type);
  SPP_STRING_END;
}

auto spp::asts::FloatLiteralAst::Stage7_AnalyseSemantics(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *)
  -> void {
  // Check the written value is one the type can hold.
  Type = Type.empty() ? "f32" : Type;
  ValidateBounds(*this, *sm);
}

auto spp::asts::FloatLiteralAst::BigVal() const
  -> numex::BigDec {
  //
  using spp::utils::strings::NormalizeFloatString;

  // The sign is a separate token, so it is applied after
  // the digits are read.
  auto value = numex::BigDec(NormalizeFloatString(IntVal->TokenData, FracVal->TokenData));
  if (TokSign != nullptr and TokSign->TokenType == lex::SppTokenType::TK_SUB) {
    value = -value;
  }
  return value;
}

auto spp::asts::FloatLiteralAst::ValidateBounds(
  Ast const &owner,
  analyse::scopes::ScopeManager const &sm) const
  -> void {
  //
  using analyse::errors::SppFloatOutOfBoundsError;

  // A value the type cannot hold is the same error whether
  // it was written down or computed by comp-time arithmetic.
  auto const &[lower, upper] = kBounds.at(Type);
  const auto value = BigVal();
  RaiseIf<SppFloatOutOfBoundsError>(
    value < lower or value > upper,
    {sm.CurrentScope}, ERR_ARGS(owner, value, lower, upper, Type));
}

auto spp::asts::FloatLiteralAst::FromBigVal(
  numex::BigDec const &value,
  Str const &type)
  -> Unique<FloatLiteralAst> {
  // "Decimal" gives the exact decimal, not in fraction form.
  const auto is_negative = value.IsNegative();
  const auto digits = (is_negative ? -value : value).Decimal(kDecimalPlaces.at(type));
  const auto point = digits.find('.');

  auto int_part = point == Str::npos ? digits : digits.substr(0, point);
  auto frac_part = point == Str::npos ? Str("0") : digits.substr(point + 1);

  auto sign_tok = is_negative
    ? MakeUnique<TokenAst>(0uz, lex::SppTokenType::TK_SUB, spp::lex::tok_to_string(lex::SppTokenType::TK_SUB))
    : nullptr;
  return MakeUnique<FloatLiteralAst>(
    std::move(sign_tok),
    MakeUnique<TokenAst>(0uz, lex::SppTokenType::LX_NUMBER, std::move(int_part)),
    MakeUnique<TokenAst>(0uz, lex::SppTokenType::TK_DOT, "."),
    MakeUnique<TokenAst>(0uz, lex::SppTokenType::LX_NUMBER, std::move(frac_part)),
    Str(type));
}

auto spp::asts::FloatLiteralAst::Stage9_CompTimeResolve(
  analyse::scopes::ScopeManager *,
  meta::CompilerMetaData *meta)
  -> void {
  // Clone and return the float literal as is for compile-time
  // resolution.
  meta->CmpResult = AstClone(this);
}

auto spp::asts::FloatLiteralAst::Stage11_CodeGen(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *meta,
  codegen::LlvmCtx *ctx)
  -> llvm::Value* {
  using spp::utils::strings::NormalizeFloatString;

  // Get the type of the float literal.
  const auto type_ast = InferType(sm, meta);
  const auto type_sym = sm->CurrentScope->GetTypeSymbol(type_ast.get());
  auto llvm_type = codegen::GetLlvmType(*type_sym, ctx);

  // If come from stage10 cmp statement, register the float
  // type here, in case it hasn't been reached yet by the
  // class prototypes.
  if (llvm_type == nullptr) {
    codegen::RegisterLlvmTypeInfo(type_sym->Type, *sm, ctx);
    llvm_type = codegen::GetLlvmType(*type_sym, ctx);
  }

  // Normalise the literal exactly as Stage7 does, then
  // apply the optional sign.
  auto const &semantics = llvm_type->getFltSemantics();
  auto mapped_val = NormalizeFloatString(IntVal->TokenData, FracVal->TokenData);
  if (TokSign != nullptr and TokSign->TokenType == lex::SppTokenType::TK_SUB) {
    mapped_val = "-" + mapped_val;
  }

  // Create the LLVM constant float value from the
  // normalised decimal string (APFloat handled the sign).
  const auto ap_float = llvm::APFloat(semantics, mapped_val);
  const auto co_float = llvm::ConstantFP::get(*ctx->Context, ap_float);
  return co_float;
}

auto spp::asts::FloatLiteralAst::InferType(
  analyse::scopes::ScopeManager *sm,
  meta::CompilerMetaData *)
  -> Shared<TypeAst> {
  //
  using analyse::errors::SppInternalCompilerError;
  using namespace generate::common_types_precompiled;

  // Map the type string literal to the correct SPP type.
  auto spp_type = static_cast<TypeAst*>(nullptr);
  if (Type.empty()) { spp_type = F32.get(); }
  else if (Type == "f8") { spp_type = F8.get(); }
  else if (Type == "f16") { spp_type = F16.get(); }
  else if (Type == "f32") { spp_type = F32.get(); }
  else if (Type == "f64") { spp_type = F64.get(); }
  else if (Type == "f128") { spp_type = F128.get(); }
  else {
    Raise<SppInternalCompilerError>(
      {sm->CurrentScope},
      ERR_ARGS(*this, "invalid float literal type"));
  }

  const auto sym = sm->CurrentScope->GetTypeSymbol(spp_type);
  return sym->FqName();
}

SPP_MOD_END
