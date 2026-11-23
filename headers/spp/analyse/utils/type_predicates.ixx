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
  /// Check if any part of a type satisfies a given predicate. Walk the type,
  /// its generics (including inner ones), passing each part into the predicate.
  /// Inner generics could be "cmp", using "AnyCompPart", whose names are asked
  /// "comp_name".
  SPP_EXP_FUN auto AnyTypePart(
    TypeAst const &type, FuncRef<bool(TypeIdentifierAst const &)> type_match,
    Func<bool(IdentifierAst const &)> const &comp_match = nullptr) -> bool;

  /// Whether any part of a comp expression satisfies a predicate, walking an
  /// operation's operands, a parenthesis, a tuple's or an array's elements, an
  /// object's arguments, and a postfix operation's operand and arguments. Each
  /// value is asked "value" before its parts, each name "name", and each type
  /// a value is named through ("Self::N", "Some[T](...)") "owner", where given.
  SPP_EXP_FUN auto AnyCompPart(
    ExpressionAst const &expr, Func<bool(IdentifierAst const &)> const &comp_match,
    Func<bool(TypeAst const &)> const &owner = nullptr,
    Func<bool(ExpressionAst const &)> const &value = nullptr) -> bool;

  /// Check if "Self" appears anywhere in a type, at any depth ("Opt[Self]",
  /// "&Self"), not only as the whole type.
  SPP_EXP_FUN auto DoesTypeNameSelf(TypeAst const &type) -> bool;

  /// Check if a type contains a given generic parameter, using the unique
  /// parameter id stored by generic parameters. Reuses the above "AnyXXX"
  /// functions.
  SPP_EXP_FUN auto DoesTypeNameAGnParam(
    TypeAst const &type, GenericParameterAst const &param,
    Scope const &scope) -> bool;

  /// Whether "sym", read in "scope", is an instance of "tmpl" (or "tmpl"
  /// itself), or extends one: the template of the type, or of any "sup" scope
  /// attached to it.
  SPP_EXP_FUN auto IsOrExtendsTemplate(
    TypeSymbol const &sym, Scope const &scope, TypeSymbol const *tmpl) -> bool;

  SPP_EXP_FUN auto IsTypeGenerator(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeTuple(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeArray(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeVariant(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeFunction(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeCompTimeIndexable(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeBool(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeVoid(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeTry(TypeRef const &ref, Scope const &scope) -> bool;

  /// Whether a sized-integer type is a signed one. Get the full integer type
  /// and check against the "sized" generic argument for "=true".
  SPP_EXP_FUN auto IsTypeSignedInteger(
    TypeAst const &type, Scope const &scope) -> bool;

  /// A concrete type is a type whose generic arguments are all concrete types
  /// ie not-generic or Self.
  SPP_EXP_FUN auto IsTypeConcrete(
    TypeAst const &type, Scope const &scope) -> bool;

  /// The "TypeRef" version of the normal "IsTypeConcrete" function, using the
  /// same method as above.
  SPP_EXP_FUN auto IsTypeConcrete(TypeRef const &ref) -> bool;

  /// A type is borrowed if it has a convention, or it's a by-move variant that
  /// itself can contain a borrow, like "Str or &S32", read in "scope".
  SPP_EXP_FUN auto IsTypeBorrowed(
    TypeAst const &type, Scope const &scope, bool deep = true) -> bool;
}
