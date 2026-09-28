module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_members;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeRef);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::utils::type_members, struct TypePart);
use(spp::asts, struct ClassAttributeAst);
use(spp::asts, struct ClassPrototypeAst);
use(spp::asts, struct CmpStatementAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TypeAst);

/// A uniform way to handle "parts" of types, making attribute
/// vs indexing a lot easier / cleaner. Provides one way to
/// handle both.
SPP_EXP_CLS struct spp::analyse::utils::type_members::TypePart {
  /// The attribute's name, or the element's index written out
  Shared<IdentifierAst> Step;

  /// Position in the walk, which for a tuple or an array is
  /// the element's index.
  std::size_t Index;

  /// The part's own type.
  Shared<TypeAst> Type;

  /// The symbol of the part's own type.
  TypeSymbol *Sym;

  /// The scope that the part's type resolves in.
  Scope const *Where;
};

namespace spp::analyse::utils::type_members {
  /// Get all the parts of a type, either the fields for a type
  /// (and its super types' fields), or the indexes for a tuple
  /// or array. Use the new type part struct.
  SPP_EXP_FUN auto GetAllParts(
    TypeSymbol const &sym,
    Scope const &scope,
    bool collapse_arrays = false)
    -> Vec<TypePart>;

  /// Get all the fields on a type, and all of it's super types,
  /// tracking the field, symbol, and scope. The scope is so
  /// we know which super class it came from if it's not on the
  /// actual type itself.
  SPP_EXP_FUN auto GetAllAttrs(
    TypeSymbol const &cls_sym)
    -> Vec<Tup<Shared<IdentifierAst>, TypeSymbol*, Scope*>>;

  /// Similar to the "GetAllAttrs", but in ast form, so that
  /// the default values can be extracted for object initializers,
  /// if required.
  SPP_EXP_FUN auto GetAllAttrAsts(
    TypeSymbol const &cls_sym)
    -> Vec<ClassAttributeAst*>;

  /// Check that all the instances of a "cmp" constant, in a
  /// type and its super types it is extending, have a consistent
  /// type.
  SPP_EXP_FUN auto CheckShadowedCmpAgreesInType(
    CmpStatementAst const &cmp_member,
    Scope &cls_scope,
    Scope const &own_scope,
    ScopeManager const &sm)
    -> void;

  /**
   * Drop everything @c GetUnimplementedAbstractMethods has remembered. Its cache is keyed on scope addresses, so it
   * must not outlive the scopes it names - a later run allocating a scope at a freed address would otherwise be
   * handed the old scope's answer.
   */
  SPP_EXP_FUN auto ClearUnimplementedAbstractMethodsCache()
    -> void;

  /// Collect all the methods on a type that are abstract,
  /// which in turn indicates the type itself is abstract.
  /// Check the abstract methods don't have same signature
  /// overrides on this type or its superimposition classes
  /// upto the base who contains abstract methods.
  SPP_EXP_FUN auto GetUnimplementedAbstractMethods(
    Scope const &type_scope)
    -> Vec<FunctionPrototypeAst const*>;

  /// Get the index of a field on its type, so that LLVM
  /// can determine which slot to push data into. Takes into
  /// account hidden fat pointer fields too.
  SPP_EXP_FUN auto GetFieldIndexInType(
    TypeSymbol const &type_sym,
    IdentifierAst const &field_name)
    -> std::size_t;

  /// The classes "sym" is superimposed as ("sup Foo ext Bar"),
  /// in the order its sup scopes list them. Empty for a type
  /// with no scope of its own (a generic parameter).
  SPP_EXP_FUN auto SuperClassTypes(TypeSymbol const &sym) -> Vec<TypeSymbol*>;

  /// The classes among "sup_scopes", each named where its own sup
  /// scope reads it: a sup type's name can hold a "Self" (as in
  /// "S32 ext Ord[Rhs=Self]") only that scope has a symbol for.
  SPP_EXP_FUN auto SuperClassNames(Vec<Scope*> const &sup_scopes) -> Vec<Pair<Shared<TypeAst>, Scope const*>>;

  /// Get the number of synthetic fat-pointer fields on this
  /// type, typically the resume_fn/env_ptr or fn_ptr/env_ptr
  /// fields prepended ahead of a type's own declared fields.
  /// The fat pointer fields are always at the start of the
  /// types for simplicity.
  SPP_EXP_FUN auto GetSuperimposedFatPointerFieldCount(TypeSymbol const &type_sym) -> std::size_t;

  /// The first type a value of @p ref holds by value (itself
  /// included) that @p matches: as a tuple or array element, a
  /// variant member, or an attribute, at any depth. A borrow or
  /// a pointer holds nothing by value ("Vec[T]" keeps its "T"s
  /// behind "RawBuf"'s pointer). Null when nothing matches.
  SPP_EXP_FUN auto FindHeldByValue(
    TypeRef const &ref,
    Scope const &scope,
    std::function<bool(TypeSymbol const &, Scope const &)> const &matches)
    -> TypeSymbol const*;

  /// Detect if a type is recursive by checking all the fields
  /// of the type recursively, and making sure a look in the
  /// type graph is never reached.
  SPP_EXP_FUN auto IsTypeRecursive(ClassPrototypeAst const &type, ScopeManager const &sm) -> Shared<TypeAst>;

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

}
