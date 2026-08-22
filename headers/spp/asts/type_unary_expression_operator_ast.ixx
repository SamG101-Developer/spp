module;
#include <spp/macros.hpp>

export module spp.asts.type_unary_expression_operator_ast;
import spp.asts.ast;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct TypeIdentifierAst;
  SPP_EXP_CLS struct TypeUnaryExpressionOperatorAst;
  SPP_EXP_CLS struct TypeUnaryExpressionOperatorBorrowAst;
  SPP_EXP_CLS struct TypeUnaryExpressionOperatorNamespaceAst;
}

SPP_EXP_CLS struct spp::asts::TypeUnaryExpressionOperatorAst : Ast {
  using Ast::Ast;

  ~TypeUnaryExpressionOperatorAst() override;

  auto operator<=>(
    TypeUnaryExpressionOperatorAst const &) const
    -> Ordering;

  auto operator==(
    TypeUnaryExpressionOperatorAst const &) const
    -> bool;

  SPP_ATTR_NODISCARD virtual auto EqualsOpBorrow(
    TypeUnaryExpressionOperatorBorrowAst const &) const
    -> Ordering;

  SPP_ATTR_NODISCARD virtual auto EqualsOpNamespace(
    TypeUnaryExpressionOperatorNamespaceAst const &) const
    -> Ordering;

  SPP_ATTR_NODISCARD virtual auto Equals(
    TypeUnaryExpressionOperatorAst const &) const
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
};
