module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_predicates;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeAst);
use(spp::asts, struct TypeIdentifierAst);

namespace spp::analyse::utils::type_predicates {
  /// Whether any part of a type satisfies a predicate: each name part ("Vec" and "T" in "Vec[T]"), walking through
  /// conventions, nested names and generic arguments. A comp argument is walked as a comp expression
  /// ("AnyCompPart"): its names are asked "comp_name", where given, and the types its constants are
  /// named through are walked as types. Walks the ast in place, with no list of parts built.
  SPP_EXP_FUN auto AnyTypePart(
    TypeAst const &type,
    std::function<bool(TypeIdentifierAst const &)> const &part,
    std::function<bool(IdentifierAst const &)> const &comp_name = nullptr)
    -> bool;

  /// Whether any part of a comp expression satisfies a predicate, walking an operation's operands, a parenthesis and a
  /// pack's elements: each name is asked "name", each type a constant is named through ("Self::N") "owner", and each
  /// value that is not a pack "value" (an element, or a part of one), where given. The comp analog of
  /// "AnyTypePart".
  SPP_EXP_FUN auto AnyCompPart(
    ExpressionAst const &expr,
    std::function<bool(IdentifierAst const &)> const &name,
    std::function<bool(TypeAst const &)> const &owner = nullptr,
    std::function<bool(ExpressionAst const &)> const &value = nullptr) -> bool;

  /// Check if "Self" appears anywhere in a type, at any depth
  /// ("Opt[Self]", "&Self"), not only as the whole type.
  SPP_EXP_FUN auto DoesTypeNameSelf(TypeAst const &type) -> bool;

  /// The kind checks for a resolved type, as a value of it is held ("TypeRef"): whether it instantiates the template
  /// of the kind ("TypeRef::IsA"). A borrow is none of the kinds (a borrowed variant excepted), "!" is only itself,
  /// and a "$" mock is a function value.
  SPP_EXP_FUN auto IsTypeGenerator(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeTuple(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeArray(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeVariant(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeFunction(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeCompTimeIndexable(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeBool(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeVoid(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeTry(TypeRef const &ref, Scope const &scope) -> bool;

  /// What a written type's head names here ("Vec" for "Vec[Str]"), as its kind is read ("TypeRef::OfKind"), held as
  /// written: no instance is looked up or made. No type for "!" or a name that does not resolve.
  SPP_EXP_FUN auto HeadKindRef(TypeAst const &type, Scope const &scope) -> TypeRef;

  /// The kind checks for a written type, read through its head ("HeadKindRef"), with the same rules as for a "TypeRef".
  SPP_EXP_FUN auto IsTypeTuple(TypeAst const &type, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeArray(TypeAst const &type, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeVariant(TypeAst const &type, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeFunction(TypeAst const &type, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeVoid(TypeAst const &type, Scope const &scope) -> bool;

  /// Whether a written type is concrete where "scope" reads it: its
  /// identity names no parameter and no "Self" ("IsConcreteTypeId",
  /// of "Scope::PartialTypeIdOf", so a name that does not resolve here does
  /// not count against it), and it is not a generic template named
  /// bare.
  SPP_EXP_FUN auto IsTypeConcrete(TypeAst const &type, Scope const &scope) -> bool;

  /// "IsTypeConcrete" on a resolved type: its identity is concrete
  /// ("IsConcreteTypeId"), and it is not a bare template.
  SPP_EXP_FUN auto IsTypeConcrete(TypeRef const &ref) -> bool;

  /// "IsTypeConcrete" for a comp value: its identity names no parameter ("IsConcreteCompId"); "n + 1_uz" is concrete
  /// with "n" bound, "w" is not in the template that declares it.
  SPP_EXP_FUN auto IsCompConcrete(ExpressionAst const &val, Scope const &scope) -> bool;

  /// A type is borrowed if it has a convention, or its a by-move
  /// variant that itself can contain a borrow, like "Str or &S32".
  SPP_EXP_FUN auto IsTypeBorrowed(TypeAst const &type, ScopeManager const &sm, bool deep = true) -> bool;

  /// Whether every generic argument is concrete ("IsTypeConcrete",
  /// "IsCompConcrete").
  SPP_EXP_FUN auto AreAllGnArgsConcrete(Vec<Unique<GenericArgumentAst>> const &args, Scope const &scope) -> bool;
}
