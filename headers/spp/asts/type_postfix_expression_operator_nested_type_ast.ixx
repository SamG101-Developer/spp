module;
#include <spp/macros.hpp>

export module spp.asts.type_postfix_expression_operator_nested_type_ast;
import spp.asts.ast_kind;
import spp.asts.token_ast;
import spp.asts.type_postfix_expression_operator_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(TypePostfixExpressionOperatorNestedTypeAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeIdentifierAst);

SPP_EXP_CLS struct spp::asts::TypePostfixExpressionOperatorNestedTypeAst final : TypePostfixExpressionOperatorAst {
  SPP_GCC_VTABLE_FIX;
  SPP_AST_KEY_FUNCTIONS(TypePostfixExpressionOperatorNestedTypeAst);

  /// The "::" namespace operator token.
  std::unique_ptr<TokenAst> TokSep;

  /// The nested type identifier being accessed within the
  /// outer type.
  std::shared_ptr<TypeIdentifierAst> Name;

  TypePostfixExpressionOperatorNestedTypeAst(
    decltype(TokSep) &&tok_sep,
    decltype(Name) name);

  ~TypePostfixExpressionOperatorNestedTypeAst() override;

  SPP_ATTR_NODISCARD auto EqualsNestedType(
    TypePostfixExpressionOperatorNestedTypeAst const &) const
    -> Ordering override;

  SPP_ATTR_NODISCARD auto Equals(TypePostfixExpressionOperatorAst const &) const -> Ordering override;

  auto NsPartsInto(Vec<IdentifierAst const*> &out) const -> void override;

  auto TypePartsInto(Vec<TypeIdentifierAst const*> &out) const -> void override;

  SPP_ATTR_NODISCARD auto NsParts() const -> Vec<IdentifierAst const*> override;

  SPP_ATTR_NODISCARD auto NsParts() -> Vec<IdentifierAst*> override;

  SPP_ATTR_NODISCARD auto TypeParts() const -> Vec<TypeIdentifierAst const*> override;

  SPP_ATTR_NODISCARD auto TypeParts() -> Vec<TypeIdentifierAst*> override;

  SPP_ATTR_NODISCARD auto LastTypePart() const -> TypeIdentifierAst const* override { return Name.get(); }

  SPP_ATTR_NODISCARD auto LastTypePart() -> TypeIdentifierAst* override { return Name.get(); }
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypePostfixExpressionOperatorNestedTypeAst)
