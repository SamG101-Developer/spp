module;
#include <spp/macros.hpp>

export module spp.asts.type_unary_expression_operator_namespace_ast;
import spp.asts.ast_kind;
import spp.asts.type_unary_expression_operator_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(TypeUnaryExpressionOperatorNamespaceAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeIdentifierAst);

SPP_EXP_CLS struct spp::asts::TypeUnaryExpressionOperatorNamespaceAst final : TypeUnaryExpressionOperatorAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(TypeUnaryExpressionOperatorNamespaceAst);

  /// The namespace in which the type is defined.
  Shared<IdentifierAst> Ns;

  /// The "::" namespace operator token.
  Unique<TokenAst> TokSep;

  explicit TypeUnaryExpressionOperatorNamespaceAst(
    decltype(Ns) ns,
    decltype(TokSep) &&tok_sep);

  ~TypeUnaryExpressionOperatorNamespaceAst() override;

  SPP_ATTR_NODISCARD auto EqualsOpNamespace(TypeUnaryExpressionOperatorNamespaceAst const &) const -> Ordering override;

  SPP_ATTR_NODISCARD auto Equals(TypeUnaryExpressionOperatorAst const &) const -> Ordering override;

  auto NsPartsInto(Vec<IdentifierAst const*> &out) const -> void override;

  auto TypePartsInto(Vec<TypeIdentifierAst const*> &out) const -> void override;

  SPP_ATTR_NODISCARD auto NsParts() const -> Vec<IdentifierAst const*> override;

  SPP_ATTR_NODISCARD auto NsParts() -> Vec<IdentifierAst*> override;

  SPP_ATTR_NODISCARD auto TypeParts() const -> Vec<TypeIdentifierAst const*> override;

  SPP_ATTR_NODISCARD auto TypeParts() -> Vec<TypeIdentifierAst*> override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypeUnaryExpressionOperatorNamespaceAst)
