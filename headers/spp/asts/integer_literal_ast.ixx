module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

export module spp.asts.integer_literal_ast;
import spp.asts.ast_kind;
import spp.asts.literal_ast;
import spp.asts.type_ast;
import spp.codegen.llvm_ctx;
import spp.lex.tokens;
import spp.utils.numbers;
import spp.utils.traits;
import spp.utils.types;
import llvm;
import std;
import sys;
import numex.big_int;

SPP_AST_COMMON_FWD_DECL(IntegerLiteralAst);
use(spp::analyse::scopes, class Scope);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);

SPP_EXP_CLS struct spp::asts::IntegerLiteralAst final : LiteralAst {
  inline static const auto kBounds = utils::numbers::IntLimitMap{
    {Str("s8"), LIMIT_S(8)},
    {Str("s16"), LIMIT_S(16)},
    {Str("s32"), LIMIT_S(32)},
    {Str("s64"), LIMIT_S(64)},
    {Str("s128"), LIMIT_S(128)},
    {Str("s256"), LIMIT_S(256)},
    {Str("sz"), LIMIT_S(sizeof(sys::ssize_t) * 8)},
    {Str("u8"), LIMIT_U(8)},
    {Str("u16"), LIMIT_U(16)},
    {Str("u32"), LIMIT_U(32)},
    {Str("u64"), LIMIT_U(64)},
    {Str("u128"), LIMIT_U(128)},
    {Str("u256"), LIMIT_U(256)},
    {Str("uz"), LIMIT_U(sizeof(std::size_t) * 8)},
  };

  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(IntegerLiteralAst);

  /// The optional sign token, either "+" or "-". No sign
  /// means the integer is positive.
  Unique<TokenAst> TokSign;

  /// The token holding the integer value in the source code.
  Unique<TokenAst> Val;

  /// The raw type of the integer literal, from its postfix
  /// tag, like "i32" or "u64".
  Str Type;

  IntegerLiteralAst(
    decltype(TokSign) &&tok_sign,
    decltype(Val) &&val,
    Str &&type);

  ~IntegerLiteralAst() override;

  SPP_ATTR_NODISCARD auto EqualsIntegerLiteral(IntegerLiteralAst const &) const -> Ordering override;

  SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;

  template <typename T> requires spp::utils::traits::integral<T>
  auto CppVal() const -> T;

  /// The exact value of this literal, at whatever width it
  /// takes. Comp-time arithmetic works in this rather than in
  /// a fixed-width C++ type, so a result which does not fit
  /// is one the compiler can see and reject, rather than one
  /// that silently wrapped on the way out.
  SPP_ATTR_NODISCARD auto BigVal() const -> numex::BigInt;

  /// Build a literal of the given type carrying an exact
  /// value, with the sign as its own token. The value is not
  /// range checked here - what produced it has no scope to
  /// report an error against - so a caller that can compute
  /// an out of range value pairs this with "ValidateBounds".
  static auto FromBigVal(numex::BigInt const &value, Str const &type) -> Unique<IntegerLiteralAst>;

  /// Build a literal the way a bit operation produces one:
  /// the value is taken within the type's own width and read
  /// back under the type's signedness, rather than kept at
  /// whatever width the exact result needed.
  static auto FromWrappedBigVal(numex::BigInt const &value, Str const &type) -> Unique<IntegerLiteralAst>;

  /// Raise if this literal's value is one its type cannot
  /// hold. A written literal is checked when it is analysed;
  /// one that comp-time arithmetic produced is checked where
  /// that arithmetic is invoked from.
  auto ValidateBounds(Ast const &owner, Scope const &scope) const -> void;

private:
  /// The precompiled type this literal's suffix names, resolved where "sm" is.
  auto _PrecompiledTypeSym(ScopeManager *sm) const -> TypeSymbol*;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::IntegerLiteralAst)
