module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.integer_literal_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.token_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_type;
import spp.lex.tokens;
import spp.utils.numbers;
import spp.utils.strings;
import spp.utils.types;
import boost;
import genex;
import llvm;
import sys;

SPP_MOD_BEGIN
static const auto kIntegerBounds = spp::utils::numbers::IntLimitMap{
  {spp::Str("s8"), LIMIT(std::int8_t)},
  {spp::Str("s16"), LIMIT(std::int16_t)},
  {spp::Str("s32"), LIMIT(std::int32_t)},
  {spp::Str("s64"), LIMIT(std::int64_t)},
  {spp::Str("s128"), LIMIT(boost::int128_t)},
  {spp::Str("s256"), LIMIT(boost::int256_t)},
  {spp::Str("sz"), LIMIT(sys::ssize_t)},
  {spp::Str("u8"), LIMIT(std::uint8_t)},
  {spp::Str("u16"), LIMIT(std::uint16_t)},
  {spp::Str("u32"), LIMIT(std::uint32_t)},
  {spp::Str("u64"), LIMIT(std::uint64_t)},
  {spp::Str("u128"), LIMIT(boost::uint128_t)},
  {spp::Str("u256"), LIMIT(boost::uint256_t)},
  {spp::Str("uz"), LIMIT(std::size_t)},
};

spp::asts::IntegerLiteralAst::IntegerLiteralAst(
  decltype(TokSign) &&tok_sign,
  decltype(Val) &&val,
  Str &&type) :
  TokSign(std::move(tok_sign)),
  Val(std::move(val)),
  Type(std::move(type)) {}

spp::asts::IntegerLiteralAst::~IntegerLiteralAst() = default;

auto spp::asts::IntegerLiteralAst::EqualsIntegerLiteral(
  IntegerLiteralAst const &other) const
  -> Ordering {
  //
  if (
    ((not TokSign and not other.TokSign) or (TokSign and other.TokSign and *TokSign == *other.TokSign))
    and Val->TokenData == other.Val->TokenData
    and Type == other.Type) {
    return Ordering::equal;
  }
  return Ordering::less;
}

auto spp::asts::IntegerLiteralAst::Equals(
  ExpressionAst const &other) const
  -> Ordering {
  // Reverse hook (double dispatch)/
  return other.EqualsIntegerLiteral(*this);
}

auto spp::asts::IntegerLiteralAst::PosStart() const
  -> std::size_t {
  // Use sign token or the value.
  return TokSign ? TokSign->PosStart() : Val->PosStart();
}

auto spp::asts::IntegerLiteralAst::PosEnd() const
  -> std::size_t {
  // Use the value.
  return Val->PosEnd();
}

auto spp::asts::IntegerLiteralAst::Clone() const
  -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<IntegerLiteralAst>(
    AstClone(TokSign),
    AstClone(Val),
    Type.c_str());
}

auto spp::asts::IntegerLiteralAst::ToString() const
  -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokSign);
  SPP_STRING_APPEND(Val);
  raw_string.append("_").append(Type);
  SPP_STRING_END;
}

auto spp::asts::IntegerLiteralAst::Stage7_AnalyseSemantics(
  ScopeManager *sm,
  CompilerMetaData *)
  -> void {
  //
  using spp::utils::strings::NormaliseIntegerString;
  using analyse::errors::SppIntegerOutOfBoundsError;

  // For oct, we need to change "0o" to "0" for boost compatibility. Replace "o" with "0".
  auto data = Val->TokenData;
  data |= genex::actions::replace('o', '0');

  // Get the lower and upper bounds as big ints.
  Type = Type.empty() ? "s32" : Type;
  auto const &[lower, upper] = kIntegerBounds.at(Type);
  auto mapped_val = boost::BigInt(NormaliseIntegerString(data));
  if (TokSign != nullptr and TokSign->TokenType == lex::SppTokenType::TK_SUB) {
    mapped_val.backend().negate();
  }

  // Check if the value is within the bounds.
  RaiseIf<SppIntegerOutOfBoundsError>(
    mapped_val.compare(lower) < 0 or mapped_val.compare(upper) > 0,
    {sm->CurrentScope}, ERR_ARGS(*this, mapped_val, lower, upper, Type));
}

auto spp::asts::IntegerLiteralAst::Stage9_CompTimeResolve(
  ScopeManager *,
  CompilerMetaData *meta)
  -> void {
  // Clone and return the float literal as is for compile-time resolution.
  meta->CmpResult = AstClone(this);
}

auto spp::asts::IntegerLiteralAst::Stage11_CodeGen(
  ScopeManager *sm,
  CompilerMetaData *meta,
  codegen::LLvmCtx *ctx)
  -> llvm::Value* {
  using spp::utils::strings::NormaliseIntegerString;

  // Get the type of the integer literal.
  const auto type_ast = InferType(sm, meta);
  const auto type_sym = sm->CurrentScope->GetTypeSymbol(type_ast.get());
  auto llvm_type = codegen::GetLlvmType(*type_sym, ctx);

  // If come from stage10 cmp statement, do the int type immediately.
  if (llvm_type == nullptr) {
    codegen::RegisterLlvmTypeInfo(type_sym->Type, ctx);
    llvm_type = codegen::GetLlvmType(*type_sym, ctx);
  }

  const auto bit_width = llvm_type->getIntegerBitWidth();

  // Normalise the literal exactly as Stage7 does, then apply the optional sign.
  auto data = Val->TokenData;
  data |= genex::actions::replace('o', '0');
  auto mapped_val = boost::BigInt(NormaliseIntegerString(data));
  if (TokSign != nullptr and TokSign->TokenType == lex::SppTokenType::TK_SUB) {
    mapped_val.backend().negate();
  }

  // Create the LLVM constant integer value from the normalised decimal string (APInt handles the sign).
  const auto ap_int = llvm::APInt(bit_width, mapped_val.str(), 10);
  const auto co_int = llvm::ConstantInt::get(*ctx->Context, ap_int);
  return co_int;
}

auto spp::asts::IntegerLiteralAst::InferType(
  ScopeManager *sm,
  CompilerMetaData *)
  -> Shared<TypeAst> {
  //
  using namespace generate::common_types_precompiled;
  using analyse::errors::SppInternalCompilerError;

  // Map the type string literal to the correct SPP type.
  auto spp_type = static_cast<TypeAst*>(nullptr);
  if (Type.empty()) { spp_type = S32.get(); }
  else if (Type == "s8") { spp_type = S8.get(); }
  else if (Type == "s16") { spp_type = S16.get(); }
  else if (Type == "s32") { spp_type = S32.get(); }
  else if (Type == "s64") { spp_type = S64.get(); }
  else if (Type == "s128") { spp_type = S128.get(); }
  else if (Type == "s256") { spp_type = S256.get(); }
  else if (Type == "sz") { spp_type = SSIZE.get(); }
  else if (Type == "u8") { spp_type = U8.get(); }
  else if (Type == "u16") { spp_type = U16.get(); }
  else if (Type == "u32") { spp_type = U32.get(); }
  else if (Type == "u64") { spp_type = U64.get(); }
  else if (Type == "u128") { spp_type = U128.get(); }
  else if (Type == "u256") { spp_type = U256.get(); }
  else if (Type == "uz") { spp_type = USIZE.get(); }
  else {
    Raise<SppInternalCompilerError>(
      {sm->CurrentScope},
      ERR_ARGS(*this, "invalid integer literal type"));
  }

  const auto sym = sm->CurrentScope->GetTypeSymbol(spp_type);
  return sym->FqName();
}

template <typename T> requires spp::utils::traits::integral<T>
auto spp::asts::IntegerLiteralAst::CppVal() const -> T {
  const auto raw_str = Val->ToString();
  const auto signed_str = TokSign != nullptr ? "-" + raw_str : raw_str;
  if constexpr (std::is_unsigned_v<T>) { return static_cast<T>(std::stoull(signed_str)); }
  else { return static_cast<T>(std::stoll(signed_str)); }
}

// Manual instantiation of.CppVal function
template auto spp::asts::IntegerLiteralAst::CppVal<std::int8_t>() const -> std::int8_t;
template auto spp::asts::IntegerLiteralAst::CppVal<std::int16_t>() const -> std::int16_t;
template auto spp::asts::IntegerLiteralAst::CppVal<std::int32_t>() const -> std::int32_t;
template auto spp::asts::IntegerLiteralAst::CppVal<std::int64_t>() const -> std::int64_t;
template auto spp::asts::IntegerLiteralAst::CppVal<boost::int128_t>() const -> boost::int128_t;
template auto spp::asts::IntegerLiteralAst::CppVal<boost::int256_t>() const -> boost::int256_t;
template auto spp::asts::IntegerLiteralAst::CppVal<std::uint8_t>() const -> std::uint8_t;
template auto spp::asts::IntegerLiteralAst::CppVal<std::uint16_t>() const -> std::uint16_t;
template auto spp::asts::IntegerLiteralAst::CppVal<std::uint32_t>() const -> std::uint32_t;
template auto spp::asts::IntegerLiteralAst::CppVal<std::uint64_t>() const -> std::uint64_t;
template auto spp::asts::IntegerLiteralAst::CppVal<boost::uint128_t>() const -> boost::uint128_t;
template auto spp::asts::IntegerLiteralAst::CppVal<boost::uint256_t>() const -> boost::uint256_t;

SPP_MOD_END
