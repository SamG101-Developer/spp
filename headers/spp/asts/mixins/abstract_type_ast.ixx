module;
#include <spp/macros.hpp>

export module spp.asts.mixins.abstract_type_ast;
import spp.utils.types;
import genex;
import std;

use(spp::asts, struct ConventionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct GenericParameterAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeIdentifierAst);
use(spp::asts::mixins, struct AbstractTypeAst);

/// The "AbstractTypeAst" is a critical core mixin for the
/// base TypeAst, providing all the methods needed for standard
/// type manipulation over the three concrete type instantiations.
/// AbstrasctTypeAst->TypeAst->Type[Identifier|Unary|Postfix]Ast.
SPP_EXP_CLS struct spp::asts::mixins::AbstractTypeAst {
  AbstractTypeAst();

  virtual ~AbstractTypeAst();

  /// Whether any part of this type satisfies a predicate. This
  /// is used to prevent calling "any_of" on a vector of parts
  /// generated from this type, which was allocation-heavy and
  /// a bottleneck. Moved to "internal" processing.
  SPP_ATTR_NODISCARD virtual auto AnyPart(std::function<bool(TypeIdentifierAst const &)> const &pred) const -> bool = 0;

  /// Because the "Never" type has unique checking, we don't want
  /// to have extras "TypeEq"s against "Never" for every standard
  /// "TypeEq", so we log it as a property on construction for a
  /// flag check.
  SPP_ATTR_NODISCARD virtual auto IsNeverType() const noexcept -> bool = 0;

  /// Again to prevent extraneous allocations, we can allocate
  /// the namespace parts into a pre-existing list, so we only
  /// make one allocation for the entire parts retrieval, not
  /// a sublist per ast that gets merged anyway.
  virtual auto NsPartsInto(Vec<IdentifierAst const*> &out) const -> void {
    for (auto const *part : NsParts()) { out.EmplaceBack(part); }
  }

  /// Again to prevent extraneous allocations, we can allocate
  /// the type parts into a pre-existing list, so we only make
  /// one allocation for the entire parts retrieval, not a
  /// sublist per ast that gets merged anyway.
  virtual auto TypePartsInto(Vec<TypeIdentifierAst const*> &out) const -> void {
    for (auto const *part : TypeParts()) { out.EmplaceBack(part); }
  }

  /// The externally produces/allocating namespace parts list,
  /// tracked through chained type asts, such as something like
  /// unary -> unary -> identifier.
  SPP_ATTR_NODISCARD virtual auto NsParts() const -> Vec<IdentifierAst const*> = 0;

  /// The externally produces/allocating namespace parts list,
  /// tracked through chained type asts, such as something like
  /// unary -> unary -> identifier.
  SPP_ATTR_NODISCARD virtual auto NsParts() -> Vec<IdentifierAst*> = 0;

  /// The externally produces/allocating type parts list, tracked
  /// through chained type asts, such as something like
  /// identifier -> postfix -> postfix.
  SPP_ATTR_NODISCARD virtual auto TypeParts() const -> Vec<TypeIdentifierAst const*> = 0;

  /// The externally produces/allocating type parts list, tracked
  /// through chained type asts, such as something like
  /// identifier -> postfix -> postfix.
  SPP_ATTR_NODISCARD virtual auto TypeParts() -> Vec<TypeIdentifierAst*> = 0;

  /// The "last type part", meaning we dont have to allocate the
  /// entire type parts list just to get the final part.
  SPP_ATTR_NODISCARD virtual auto LastTypePart() const -> TypeIdentifierAst const* = 0;

  /// The "last type part", meaning we dont have to allocate the
  /// entire type parts list just to get the final part.
  SPP_ATTR_NODISCARD virtual auto LastTypePart() -> TypeIdentifierAst* = 0;

  /// Create a new type, by stripping the convention off of an
  /// existing type, converting something like "&std::string::Str"
  /// into "std::string::Str".
  SPP_ATTR_NODISCARD virtual auto WithoutConvention() const -> Shared<const TypeAst> = 0;

  /// View the convention on a type, such as "&" from the type
  /// "&std::string::Str".
  SPP_ATTR_NODISCARD virtual auto GetConvention() const -> ConventionAst* = 0;

  /// Create a new type, by adding a convention onto the type
  /// convertnig something like "std::string::Str" + "&" into
  /// "&std::string::Str".
  SPP_ATTR_NODISCARD virtual auto WithConvention(Unique<ConventionAst> &&conv) const -> Shared<TypeAst> = 0;

  /// Create a new type by stripping off all the generics on
  /// the type, converting "std::vector::Vec[S32]" into
  /// "std::vector::Vec".
  SPP_ATTR_NODISCARD virtual auto WithoutGenerics() const -> Shared<TypeAst> = 0;

  /// Substitute all the generics on a type given a generic
  /// argument (keyword arguments), converting something like
  /// "Vec[T]" + "[T=S32]" into "Vec[S32]".
  SPP_ATTR_NODISCARD virtual auto SubstituteGenerics(Vec<GenericArgumentAst*> const &args) const -> Shared<TypeAst> = 0;

  /// Check if a type contains a generic parameter. For example,
  /// "Vec[T]" contains "T". Used to check for unbound generics
  /// in "sup" blocks (ie never inferable).
  SPP_ATTR_NODISCARD virtual auto ContainsGenerics(GenericParameterAst const &generic) const -> bool = 0;

  /// Create a new type by adding a generic argument group onto
  /// the type, converting something like "Vec" + "[T=S32]" into
  /// "Vec[T=S32]". Not substitution based.
  SPP_ATTR_NODISCARD virtual auto WithGenerics(Unique<GenericArgumentGroupAst> &&arg_group) const
    -> Shared<TypeAst> =0;

  /// If the type name starts with a "$", then it is a compiler
  /// generated type. Unary type expressions will move into
  /// their right-hand-side identifier etc, to inspect it.
  SPP_ATTR_NODISCARD virtual auto IsCompilerGeneratedType() const -> bool = 0;

  /// Reset caches to force re-analysis when things like generics
  /// change etc.
  virtual auto ResetCache() -> void = 0;
};
