module;
#include <spp/macros.hpp>

export module spp.asts.mixins.abstract_type_ast;
import spp.utils.types;
import genex;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct ConventionAst;
  SPP_EXP_CLS struct GenericArgumentAst;
  SPP_EXP_CLS struct GenericArgumentGroupAst;
  SPP_EXP_CLS struct GenericParameterAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct TypeAst;
  SPP_EXP_CLS struct TypeIdentifierAst;
}

namespace spp::asts::mixins {
  SPP_EXP_CLS struct AbstractTypeAst;
}

SPP_EXP_CLS struct spp::asts::mixins::AbstractTypeAst {
  AbstractTypeAst();

  virtual ~AbstractTypeAst();

  /**
   * Whether any part of this type satisfies @p pred .
   *
   * @n
   * A type is a chain of nodes, and every caller of this asks a yes-or-no question about the parts rather than wanting
   * the parts themselves. Answering with a container made each level allocate one and copy the level below into it -
   * the same cost @c NsPartsInto exists to avoid - and, because the container held shared pointers, the walk also had
   * to take a @c shared_from_this of each node, so it threw on any type owned outright rather than shared. Asking the
   * question directly costs no allocation, stops at the first part that answers it, and works whoever owns the node.
   *
   * @param pred Applied to each part in turn; the walk stops at the first that returns true.
   * @return Whether any part satisfied @p pred .
   */
  SPP_ATTR_NODISCARD virtual auto AnyPart(
    std::function<bool(TypeIdentifierAst const&)> const &pred) const -> bool = 0;

  SPP_ATTR_NODISCARD virtual auto IsNeverType() const noexcept
    -> bool = 0;

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
    -> TypeIdentifierAst const* = 0;

  SPP_ATTR_NODISCARD virtual auto LastTypePart()
    -> TypeIdentifierAst* = 0;

  SPP_ATTR_NODISCARD virtual auto WithoutConvention() const
    -> Shared<const TypeAst> = 0;

  SPP_ATTR_NODISCARD virtual auto GetConvention() const
    -> ConventionAst* = 0;

  SPP_ATTR_NODISCARD virtual auto WithConvention(
    Unique<ConventionAst> &&conv) const
    -> Shared<TypeAst> = 0;

  SPP_ATTR_NODISCARD virtual auto WithoutGenerics() const
    -> Shared<TypeAst> = 0;

  SPP_ATTR_NODISCARD virtual auto SubstituteGenerics(
    Vec<GenericArgumentAst*> const &args) const
    -> Shared<TypeAst> = 0;

  SPP_ATTR_NODISCARD virtual auto ContainsGenerics(
    GenericParameterAst const &generic) const
    -> bool = 0;

  SPP_ATTR_NODISCARD virtual auto WithGenerics(
    Unique<GenericArgumentGroupAst> &&arg_group) const
    -> Shared<TypeAst> = 0;

  SPP_ATTR_NODISCARD virtual auto IsCompilerGeneratedType() const
    -> bool = 0;

  virtual auto ResetCache()
    -> void = 0;
};
