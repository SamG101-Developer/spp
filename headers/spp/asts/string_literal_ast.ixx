module;
#include <spp/macros.hpp>

export module spp.asts.string_literal_ast;
import spp.asts.ast_kind;
import spp.asts.literal_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(StringLiteralAst) {
  SPP_EXP_CLS struct TokenAst;
  SPP_EXP_CLS struct TypeAst;
}

SPP_EXP_CLS struct spp::asts::StringLiteralAst final : LiteralAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(StringLiteralAst);

  /**
   * The optional "b" prefix, converting the char into a byte string.
   */
  Unique<TokenAst> BytePrefix;

  /**
   * The string value of the string literal. This is the actual string that is represented by the literal.
   */
  Unique<TokenAst> Val;

  /**
   * Construct the StringLiteralAst with the arguments matching the members.
   * @param[in] byte_prefix The optional byte prefix of the string literal (e.g., 'b' for byte literals).
   * @param[in] val The string value of the string literal.
   */
  explicit StringLiteralAst(
    decltype(BytePrefix) &&byte_prefix,
    decltype(Val) &&val);

  ~StringLiteralAst() override;

  SPP_ATTR_NODISCARD auto EqualsStringLiteral(
    StringLiteralAst const &) const
    -> Ordering override;

  SPP_ATTR_NODISCARD auto Equals(
    ExpressionAst const &other) const
    -> Ordering override;

  auto Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* override;

  auto InferType(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Shared<TypeAst> override;

  SPP_ATTR_NODISCARD auto CppVal() const -> Str;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::StringLiteralAst)
