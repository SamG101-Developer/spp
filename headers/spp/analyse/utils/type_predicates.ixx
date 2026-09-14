module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_predicates;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::asts, struct ClassPrototypeAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct TypeAst);

namespace spp::analyse::utils::type_predicates {
  /// The only two compile-time indexable types are the
  /// array and tuple types. For now, directly these two
  /// types (Arr, Tup), not superimpositions.
  SPP_EXP_FUN auto IsTypeCompTimeIndexable(TypeAst const &type, Scope const &scope) -> bool;

  /// Check fi the type is "Self". Todo: check on the
  /// difference between this and the TypeAst IsSelf method.
  SPP_EXP_FUN auto IsTypeSelf(TypeAst const &type) -> bool;

  /// Check if a type is the array type. Strip generics and
  /// TypeEq against the non-generic Arr type.
  SPP_EXP_FUN auto IsTypeArr(TypeAst const &type, Scope const &scope) -> bool;

  /// Check if a type is the tuple type. Strip generics and
  /// TypeEq against the non-generic Tup type.
  SPP_EXP_FUN auto IsTypeTup(TypeAst const &type, Scope const &scope) -> bool;

  /// Check if a type symbol is the tuple type symbol. Used
  /// to optimize tuple-[early return guards].
  SPP_EXP_FUN auto IsTupSymbol(TypeSymbol const &sym) -> bool;

  /// Check if a type is the variant type. Strip generics
  /// *and conventions* and TypeEq against the non-generic
  /// Var type.
  SPP_EXP_FUN auto IsTypeVariant(TypeAst const &type, Scope const &scope) -> bool;

  /// Check if a type is the bool type. TypeEq against the
  /// Bool type.
  SPP_EXP_FUN auto IsTypeBool(TypeAst const &type, Scope const &scope) -> bool;

  /// Check if a type is the void type. TypeEq against the
  /// Void type.
  SPP_EXP_FUN auto IsTypeVoid(TypeAst const &type, Scope const &scope) -> bool;

  /// Check if a type is the try type. Strip generics and
  /// TypeEq against the non-generic try type.
  SPP_EXP_FUN auto IsTypeTry(TypeAst const &type, Scope const &scope) -> bool;

  /// Check if a type is one of the generator types. Strip
  /// generics and TypeEq against the non-generic Gen and
  /// GenOnce types.
  SPP_EXP_FUN auto IsTypeGen(TypeAst const &type, Scope const &scope) -> bool;

  /// Check if a type is the never type. TypeEq against the
  /// Never type.
  SPP_EXP_FUN auto IsTypeNever(TypeAst const &type, Scope const &scope) -> bool;

  /// Check if a type is one of the functional types. Strip
  /// generics and TypeEq against the non-generic FunMov,
  /// FunMut and FunRef types.
  SPP_EXP_FUN auto IsTypeFunc(TypeAst const &type, Scope const &scope) -> bool;

  /// Get the number of synthetic fat-pointer fields on this
  /// type, typically the resume_fn/env_ptr or fn_ptr/env_ptr
  /// fields prepended ahead of a type's own declared fields.
  /// The fat pointer fields are always at the start of the
  /// types for simplicity.
  SPP_EXP_FUN auto GetSuperimposedFatPointerFieldCount(TypeAst const &type, Scope const &scope) -> std::size_t;

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
    TypeAst const &type,
    Scope const &scope)
    -> Pair<bool, std::size_t>;

  /// Get the nth type of a tuple, or for an array, all the types
  /// are the same.
  SPP_EXP_FUN auto GetNthTypeOfIndexableType(
    std::size_t index,
    TypeAst const &type,
    Scope const &scope)
    -> Shared<TypeAst>;

  /// Reuse the concrete checker to check if all the generic
  /// arguments are concrete, and aren't self bound or bound
  /// to other generics.
  SPP_EXP_FUN auto AreGenericArgsConcrete(Vec<Unique<GenericArgumentAst>> const &args, Scope const &scope) -> bool;
}
