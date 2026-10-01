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
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct TypeAst);

namespace spp::analyse::utils::type_predicates {
  /// Check if "Self" appears anywhere in a type, at any depth
  /// ("Opt[Self]", "&Self"), not only as the whole type.
  SPP_EXP_FUN auto NamesSelfType(TypeAst const &type) -> bool;

  /// The kind checks above for a symbol: they test the template it stands for ("TemplateOf"), rather than reading its
  /// name back.
  SPP_EXP_FUN auto IsTypeGen(TypeSymbol const &sym, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeTup(TypeSymbol const &sym, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeArr(TypeSymbol const &sym, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeVariant(TypeSymbol const &sym, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeFunc(TypeSymbol const &sym, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeCompTimeIndexable(TypeSymbol const &sym, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeBool(TypeSymbol const &sym, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeVoid(TypeSymbol const &sym, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeTry(TypeSymbol const &sym, Scope const &scope) -> bool;

  /// The kind checks for a resolved type, as a value of it is held ("TypeRef"); a written type is read through its head
  /// ("TypeRef::OfHead"). A borrow is none of the kinds (a borrowed variant excepted), "!" is only itself, and a "$"
  /// mock is a function value.
  SPP_EXP_FUN auto IsTypeGen(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeTup(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeArr(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeVariant(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeFunc(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeCompTimeIndexable(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeBool(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeVoid(TypeRef const &ref, Scope const &scope) -> bool;
  SPP_EXP_FUN auto IsTypeTry(TypeRef const &ref, Scope const &scope) -> bool;

  /// Check if a type is fully generically-substituted, checking
  /// all the fields recursively for any generic types still in
  /// use / unbound.
  SPP_EXP_FUN auto IsTypeFullyConcrete(TypeAst const &type, Scope const &scope) -> bool;

  /// "IsTypeFullyConcrete" on a resolved type: its identity names no
  /// type or comp parameter, no binding to nothing, and no "Self".
  SPP_EXP_FUN auto IsTypeFullyConcrete(TypeRef const &ref) -> bool;

  /// A type is borrowed if it has a convention, or its a by-move
  /// variant that itself can contain a borrow, like "Str or &S32".
  SPP_EXP_FUN auto IsTypeBorrowed(TypeAst const &type, ScopeManager const &sm, bool deep = true) -> bool;

  /// Reuse the concrete checker to check if all the generic
  /// arguments are concrete, and aren't self bound or bound
  /// to other generics.
  SPP_EXP_FUN auto AreGenericArgsConcrete(Vec<Unique<GenericArgumentAst>> const &args, Scope const &scope) -> bool;
}
