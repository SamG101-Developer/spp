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
use(spp::asts, struct GenericParameterAst);
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
    FuncRef<bool(TypeIdentifierAst const &)> part,
    Func<bool(IdentifierAst const &)> const &comp_name = nullptr)
    -> bool;

  /// Whether any part of a comp expression satisfies a predicate, walking an operation's operands, a parenthesis and a
  /// pack's elements: each name is asked "name", each type a constant is named through ("Self::N") "owner", and each
  /// value that is not a pack "value" (an element, or a part of one), where given. The comp analog of
  /// "AnyTypePart".
  SPP_EXP_FUN auto AnyCompPart(
    ExpressionAst const &expr,
    Func<bool(IdentifierAst const &)> const &name,
    Func<bool(TypeAst const &)> const &owner = nullptr,
    Func<bool(ExpressionAst const &)> const &value = nullptr) -> bool;

  /// Check if "Self" appears anywhere in a type, at any depth
  /// ("Opt[Self]", "&Self"), not only as the whole type.
  SPP_EXP_FUN auto DoesTypeNameSelf(TypeAst const &type) -> bool;

  /// Whether a type names a generic parameter, by identity: a
  /// part (or a comp name) that is that parameter where it is
  /// read in "scope" ("Vec[T]" names "T"; a "T" of another
  /// declaration does not). Used to find a "sup" block's
  /// parameters its type never names (so never inferable), and
  /// the constraints others are inferred through.
  SPP_EXP_FUN auto DoesTypeNameAGnParam(
    TypeAst const &type, GenericParameterAst const &param, Scope const &scope) -> bool;

  /// Whether "sym", read in "scope", is an instance of "tmpl" (or
  /// "tmpl" itself), or extends one: the template of the type, or
  /// of any "sup" scope attached to it.
  SPP_EXP_FUN auto IsOrExtendsTemplate(
    TypeSymbol const &sym, Scope const &scope,
    TypeSymbol const *tmpl) -> bool;

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


  /// Whether a written type is concrete where "scope" reads it: its
  /// identity names no parameter and no "Self" ("IsConcreteTypeId",
  /// of "Scope::PartialTypeIdOf", so a name that does not resolve here does
  /// not count against it), and it is not a generic template named
  /// bare.
  SPP_EXP_FUN auto IsTypeConcrete(TypeAst const &type, Scope const &scope) -> bool;

  /// "IsTypeConcrete" on a resolved type: its identity is concrete
  /// ("IsConcreteTypeId"), and it is not a bare template.
  SPP_EXP_FUN auto IsTypeConcrete(TypeRef const &ref) -> bool;

  /// A type is borrowed if it has a convention, or its a by-move
  /// variant that itself can contain a borrow, like "Str or &S32".
  SPP_EXP_FUN auto IsTypeBorrowed(TypeAst const &type, ScopeManager const &sm, bool deep = true) -> bool;

}
