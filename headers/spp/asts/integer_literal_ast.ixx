module;
#include <spp/macros.hpp>

export module spp.asts.integer_literal_ast;
import spp.asts.literal_ast;
import spp.asts.type_ast;
import spp.codegen.llvm_ctx;
import spp.lex.tokens;
import spp.utils.traits;
import spp.utils.types;
import boost;
import llvm;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct IntegerLiteralAst;
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::IntegerLiteralAst final : LiteralAst {
  SPP_GCC_VTABLE_FIX

  /**
   * The optionally provided sign token. This can be either a @c + or @c - sign, indicating the sign of the integer
   * literal. No sign means the integer is positive by default.
   */
  Unique<TokenAst> TokSign;

  /**
   * The token that represents the integer literal. This is the actual integer value in the source code.
   */
  Unique<TokenAst> Val;

  /**
   * The raw type of the integer literal. This is from the postfix tag to the literal, like "i32" or "u64".
   */
  Str Type;

  /**
   * Construct the IntegerLiteralAst with the arguments matching the members.
   * @param[in] tok_sign The optionally provided sign token.
   * @param[in] val The token that represents the integer literal.
   * @param[in] type The type of the integer literal.
   */
  IntegerLiteralAst(
    decltype(TokSign) &&tok_sign,
    decltype(Val) &&val,
    Str &&type);

  ~IntegerLiteralAst() override;

  SPP_ATTR_NODISCARD auto EqualsIntegerLiteral(
    IntegerLiteralAst const &) const
    -> Ordering override;

  SPP_ATTR_NODISCARD auto Equals(
    ExpressionAst const &other) const
    -> Ordering override;

  SPP_AST_KEY_FUNCTIONS;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  template <typename T> requires utils::traits::integral<T>
  auto CppVal() const -> T;

  /**
   * The exact value of this literal, at whatever width it takes. Comp-time arithmetic works in this rather than in a
   * fixed-width C++ type, so that a result which does not fit is a result the compiler can see and reject rather than
   * one that silently wrapped on the way out.
   * @return The literal's value.
   */
  SPP_ATTR_NODISCARD auto BigVal() const -> boost::BigInt;

  /**
   * Build a literal of the given type carrying an exact value, with the sign as its own token. The value is not
   * range checked here - what produced it has no scope to report an error against - so a caller that can compute an
   * out of range value pairs this with @c ValidateBounds .
   * @param value The value the literal is to carry.
   * @param type The integer type name ("s32", "u8", ...).
   * @return The literal.
   */
  static auto FromBigVal(boost::BigInt const &value, Str const &type) -> Unique<IntegerLiteralAst>;

  /**
   * Raise if this literal's value is one its type cannot hold. A written literal is checked when it is analysed; one
   * that comp-time arithmetic produced is checked where that arithmetic is invoked from.
   * @param owner The ast to report the error against.
   * @param sm The scope manager, for error reporting.
   */
  auto ValidateBounds(Ast const &owner, ScopeManager const &sm) const -> void;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::IntegerLiteralAst)
