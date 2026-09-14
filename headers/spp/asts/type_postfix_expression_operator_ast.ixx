module;
#include <spp/macros.hpp>

export module spp.asts.type_postfix_expression_operator_ast;
import spp.asts.ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(TypePostfixExpressionOperatorAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeIdentifierAst);
use(spp::asts, struct TypePostfixExpressionOperatorNestedTypeAst);

SPP_EXP_CLS struct spp::asts::TypePostfixExpressionOperatorAst : Ast {
  TypePostfixExpressionOperatorAst();

  ~TypePostfixExpressionOperatorAst() override;

  auto operator<=>(TypePostfixExpressionOperatorAst const &) const -> Ordering;

  auto operator==(TypePostfixExpressionOperatorAst const &) const -> bool;

  SPP_ATTR_NODISCARD virtual auto EqualsNestedType(
    TypePostfixExpressionOperatorNestedTypeAst const &) const
    -> Ordering;

  SPP_ATTR_NODISCARD virtual auto Equals(TypePostfixExpressionOperatorAst const &) const -> Ordering = 0;

  /// Append this node's namespace parts to "out", rather than
  /// returning a container of its own. A type is a chain of
  /// nodes, each concatenating what the nodes below it
  /// produced, so a value-returning walk allocates a vector
  /// per level and copies each level's result into the next;
  /// appending into one buffer makes the whole chain a single
  /// allocation. "NsParts" is the same walk with the buffer
  /// supplied for the caller.
  virtual auto NsPartsInto(Vec<IdentifierAst const*> &out) const -> void {
    for (auto const *part : NsParts()) { out.EmplaceBack(part); }
  }

  virtual auto TypePartsInto(Vec<TypeIdentifierAst const*> &out) const -> void {
    for (auto const *part : TypeParts()) { out.EmplaceBack(part); }
  }

  SPP_ATTR_NODISCARD virtual auto NsParts() const -> Vec<IdentifierAst const*> = 0;

  SPP_ATTR_NODISCARD virtual auto NsParts() -> Vec<IdentifierAst*> = 0;

  SPP_ATTR_NODISCARD virtual auto TypeParts() const -> Vec<TypeIdentifierAst const*> = 0;

  SPP_ATTR_NODISCARD virtual auto TypeParts() -> Vec<TypeIdentifierAst*> = 0;

  SPP_ATTR_NODISCARD virtual auto LastTypePart() const -> TypeIdentifierAst const* { return nullptr; }

  SPP_ATTR_NODISCARD virtual auto LastTypePart() -> TypeIdentifierAst* { return nullptr; }
};
