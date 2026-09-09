module;
#include <spp/macros.hpp>

export module spp.asts.type_postfix_expression_operator_ast;
import spp.asts.ast;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct TypeIdentifierAst;
  SPP_EXP_CLS struct TypePostfixExpressionOperatorAst;
  SPP_EXP_CLS struct TypePostfixExpressionOperatorNestedTypeAst;
}

SPP_EXP_CLS struct spp::asts::TypePostfixExpressionOperatorAst : Ast {
  SPP_GCC_VTABLE_FIX

  TypePostfixExpressionOperatorAst();

  ~TypePostfixExpressionOperatorAst() override;

  auto operator<=>(
    TypePostfixExpressionOperatorAst const &) const
    -> Ordering;

  auto operator==(
    TypePostfixExpressionOperatorAst const &) const
    -> bool;

  SPP_ATTR_NODISCARD virtual auto EqualsNestedType(
    TypePostfixExpressionOperatorNestedTypeAst const &) const
    -> Ordering;

  SPP_ATTR_NODISCARD virtual auto Equals(
    TypePostfixExpressionOperatorAst const &) const
    -> Ordering = 0;

  /**
   * Append this node's namespace parts to @p out , rather than answering with a container of its own. A type is a chain
   * of nodes and each one concatenates what the nodes below it produced, so a value-returning walk allocates a vector
   * per level and copies each level's result into the next; appending into one buffer makes the whole chain a single
   * allocation. @c NsParts is the same walk with the buffer supplied for the caller.
   */
  virtual auto NsPartsInto(Vec<IdentifierAst const*> &out) const
    -> void { for (auto const *part : NsParts()) { out.EmplaceBack(part); } }

  virtual auto TypePartsInto(Vec<TypeIdentifierAst const*> &out) const
    -> void { for (auto const *part : TypeParts()) { out.EmplaceBack(part); } }

  SPP_ATTR_NODISCARD virtual auto NsParts() const
    -> Vec<IdentifierAst const*> = 0;

  SPP_ATTR_NODISCARD virtual auto NsParts()
    -> Vec<IdentifierAst*> = 0;

  SPP_ATTR_NODISCARD virtual auto TypeParts() const
    -> Vec<TypeIdentifierAst const*> = 0;

  SPP_ATTR_NODISCARD virtual auto TypeParts()
    -> Vec<TypeIdentifierAst*> = 0;

  SPP_ATTR_NODISCARD virtual auto LastTypePart() const
    -> TypeIdentifierAst const* { return nullptr; }

  SPP_ATTR_NODISCARD virtual auto LastTypePart()
    -> TypeIdentifierAst* { return nullptr; }
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypePostfixExpressionOperatorAst)
