module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_predicates;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct ClassPrototypeAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct TypeAst);

namespace spp::analyse::utils::type_predicates {
  /// Check if a type symbol is the tuple type symbol. Used
  /// to optimize tuple-[early return guards].
  SPP_EXP_FUN auto IsTupSymbol(TypeSymbol const &sym) -> bool;

  /// Check if "Self" appears anywhere in a type, at any depth
  /// ("Opt[Self]", "&Self"), not only as the whole type.
  SPP_EXP_FUN auto NamesSelfType(TypeAst const &type) -> bool;

  /// The template a symbol stands for where "scope" reads it ("Vec" for "Vec[Str]"), by the path its "FqName" takes:
  /// a parameter is what the scope binds it to ("Scope::Canon"), a binding or "Self" the type it names, an alias its
  /// target. A template, or a plain class, is its own.
  SPP_EXP_FUN auto TemplateOf(TypeSymbol const &sym, Scope const &scope) -> TypeSymbol*;

  /// Whether a symbol stands for the template a written type names ("Copy", or "Vec" for "Vec[Str]"), both taken to
  /// the template they stand for, rather than comparing names.
  SPP_EXP_FUN auto IsTemplate(TypeSymbol const &sym, TypeAst const &tmpl, Scope const &scope) -> bool;

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

  /// Get the number of synthetic fat-pointer fields on this
  /// type, typically the resume_fn/env_ptr or fn_ptr/env_ptr
  /// fields prepended ahead of a type's own declared fields.
  /// The fat pointer fields are always at the start of the
  /// types for simplicity.
  SPP_EXP_FUN auto GetSuperimposedFatPointerFieldCount(TypeSymbol const &type_sym) -> std::size_t;

  /// Detect if a type is recursive by checking all the fields
  /// of the type recursively, and making sure a look in the
  /// type graph is never reached.
  SPP_EXP_FUN auto IsTypeRecursive(ClassPrototypeAst const &type, ScopeManager const &sm) -> Shared<TypeAst>;

  /// Check if a type is fully generically-substituted, checking
  /// all the fields recursively for any generic types still in
  /// use / unbound.
  SPP_EXP_FUN auto IsTypeFullyConcrete(TypeAst const &type, Scope const &scope) -> bool;

  /// A type is borrowed if it has a convention, or its a by-move
  /// variant that itself can contain a borrow, like "Str or &S32".
  SPP_EXP_FUN auto IsTypeBorrowed(TypeAst const &type, ScopeManager const &sm, bool deep = true) -> bool;

  /// Check if an index is within the bounds of an array or tuple,
  /// ie at compile-time check if the element requested is
  /// genuinely reachable.
  SPP_EXP_FUN auto IsIndexWithinBound(
    std::size_t index,
    TypeRef const &ref,
    Scope const &scope)
    -> Pair<bool, std::size_t>;

  /// Get the nth type of a tuple, or for an array, all the types
  /// are the same.
  SPP_EXP_FUN auto GetNthTypeOfIndexableType(
    std::size_t index,
    TypeRef const &ref,
    Scope const &scope)
    -> Shared<TypeAst>;

  /// Reuse the concrete checker to check if all the generic
  /// arguments are concrete, and aren't self bound or bound
  /// to other generics.
  SPP_EXP_FUN auto AreGenericArgsConcrete(Vec<Unique<GenericArgumentAst>> const &args, Scope const &scope) -> bool;
}
