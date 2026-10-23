module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

export module spp.asts.float_literal_ast;
import spp.asts.ast_kind;
import spp.asts.literal_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;
import numex.big_dec;
import numex.big_int;

SPP_AST_COMMON_FWD_DECL(FloatLiteralAst);
use(spp::analyse::scopes, class Scope);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);

/// A floating-point literal, with an optional sign, an integer
/// part, a decimal point, and a fractional part. The type can be
/// given with a postfix type annotation, such as "_f32" or
/// "_f64"; no postfix defaults the type to "std::BigDec".
SPP_EXP_CLS struct spp::asts::FloatLiteralAst final : LiteralAst {
  SPP_GCC_VTABLE_FIX
  SPP_AST_KEY_FUNCTIONS(FloatLiteralAst);

  /// The optional sign of the float literal, either "+" or "-".
  Unique<TokenAst> TokSign;

  /// The integer part, before the decimal point. It is required
  /// (".1" is not a valid float literal).
  Unique<TokenAst> IntVal;

  /// The decimal point token, separating the integer part from
  /// the fractional part.
  Unique<TokenAst> TokDot;

  /// The fractional part, after the decimal point. It is required
  /// ("1." is not a valid float literal).
  Unique<TokenAst> FracVal;

  /// The optional type annotation, used if the default
  /// "std::BigDec" is not desired, such as "_f32" or "_f64".
  Str Type;

  FloatLiteralAst(
    decltype(TokSign) &&tok_sign,
    decltype(IntVal) &&int_val,
    decltype(TokDot) &&tok_dot,
    decltype(FracVal) &&frac_val,
    Str &&type);

  ~FloatLiteralAst() override;

  SPP_ATTR_NODISCARD auto EqualsFloatLiteral(FloatLiteralAst const &) const -> Ordering override;

  SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  /// The exact value of this literal. Comp-time arithmetic works
  /// in this rather than in a fixed-width C++ float, so that a
  /// result the type cannot hold arrives as a value the compiler
  /// can reject rather than as an infinity. An "f32" or "f64" is
  /// folded on its IEEE value instead ("utils::numbers"), read
  /// from its text.
  SPP_ATTR_NODISCARD auto BigVal() const -> numex::BigDec;

  /// A literal of "type" from "text" ("[-]digits[.digits]"), with
  /// the sign as its own token, kept on a zero too.
  static auto FromText(
    Str const &text, Str const &type) -> Unique<FloatLiteralAst>;

  /// Build a literal of the given type carrying an exact value,
  /// with the sign as its own token. The value is not range
  /// checked here - what produced it has no scope to report an
  /// error against - so a caller that can compute an out of range
  /// value pairs this with "ValidateBounds". Printed to as many
  /// places as an exact value of the type can need.
  static auto FromBigVal(
    numex::BigDec const &value, Str const &type) -> Unique<FloatLiteralAst>;

  /// Raise if this literal's value is one its type cannot hold. A
  /// written literal is checked when it is analysed; one that
  /// comp-time arithmetic produced is checked where that
  /// arithmetic is invoked from.
  auto ValidateBounds(
    Ast const &owner) const -> void;

private:
  /// The precompiled type this literal's suffix names ("S32" for "1", "U8" for "1_u8").
  auto _PrecompiledType(
    ScopeManager *sm) const -> TypeAst const&;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::FloatLiteralAst)
