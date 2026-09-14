module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

export module spp.asts.float_literal_ast;
import spp.asts.ast_kind;
import spp.asts.literal_ast;
import spp.codegen.llvm_ctx;
import spp.utils.numbers;
import spp.utils.types;
import llvm;
import std;
import numex.big_dec;
import numex.big_int;

SPP_AST_COMMON_FWD_DECL(FloatLiteralAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);

/// A floating-point literal, with an optional sign, an integer
/// part, a decimal point, and a fractional part. The type can be
/// given with a postfix type annotation, such as "_f32" or
/// "_f64"; no postfix defaults the type to "std::BigDec".
SPP_EXP_CLS struct spp::asts::FloatLiteralAst final : LiteralAst {
  inline static const auto kBounds = utils::numbers::FloatLimitMap{
    {Str("f8"), MakePair(numex::BigDec("-448"), numex::BigDec("448"))},
    {Str("f16"), LIMIT_F(11, 16)},
    {Str("f32"), LIMIT_F(24, 128)},
    {Str("f64"), LIMIT_F(53, 1024)},
    {Str("f128"), LIMIT_F(113, 16384)}
  };

  /// How many fractional digits it takes to write any value of
  /// each type exactly, which is the exponent of its smallest
  /// subnormal: every representable value is a multiple of that,
  /// so its decimal expansion terminates by then. A comp-time
  /// division can still produce a recurring value, and this is
  /// where that one gets cut short.
  inline static const auto kDecimalPlaces = Map<Str, std::uint64_t>{
    {Str("f8"), 16},
    {Str("f16"), 32},
    {Str("f32"), 160},
    {Str("f64"), 1100},
    {Str("f128"), 16500}
  };

  SPP_GCC_VTABLE_FIX;
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

  /// The exact value of this literal. Comp-time arithmetic works
  /// in this rather than in a fixed-width C++ float, so that a
  /// result the type cannot hold arrives as a value the compiler
  /// can reject rather than as an infinity.
  SPP_ATTR_NODISCARD auto BigVal() const -> numex::BigDec;

  /// Build a literal of the given type carrying an exact value,
  /// with the sign as its own token. The value is not range
  /// checked here - what produced it has no scope to report an
  /// error against - so a caller that can compute an out of range
  /// value pairs this with "ValidateBounds".
  static auto FromBigVal(numex::BigDec const &value, Str const &type) -> Unique<FloatLiteralAst>;

  /// Raise if this literal's value is one its type cannot hold. A
  /// written literal is checked when it is analysed; one that
  /// comp-time arithmetic produced is checked where that
  /// arithmetic is invoked from.
  auto ValidateBounds(Ast const &owner, ScopeManager const &sm) const -> void;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::FloatLiteralAst)
