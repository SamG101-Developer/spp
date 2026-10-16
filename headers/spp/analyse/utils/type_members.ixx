module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_members;
import spp.analyse.scopes.symbols;
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

  /// The part's own type, as "Where" reads it.
  TypeRef Ref;

  /// The scope that the part's type resolves in.
  Scope const *Where;
};

namespace spp::analyse::utils::type_members {
  /// Get all the parts of a type, either the fields for a type
  /// (and its super types' fields), or the indexes for a tuple
  /// or array, of a type as a value of it holds them (a borrow's
  /// parts are those of the type borrowed). Use the new type part
  /// struct.
  SPP_EXP_FUN auto GetAllParts(
    TypeRef const &ref, Scope const &scope, bool collapse_arrays = false) -> Vec<TypePart>;

  /// Get all the fields on a type, and all of it's super types,
  /// tracking the field, its type, and scope. The scope is so
  /// we know which super class it came from if it's not on the
  /// actual type itself. The type is read there, so it names
  /// what this instance's bindings make it.
  SPP_EXP_FUN auto GetAllAttrs(
    TypeSymbol const &cls_sym) -> Vec<Tup<Shared<IdentifierAst>, TypeRef, Scope*>>;

  /// Similar to the "GetAllAttrs", but in ast form, so that
  /// the default values can be extracted for object initializers,
  /// if required.
  SPP_EXP_FUN auto GetAllAttrAsts(
    TypeSymbol const &cls_sym) -> Vec<ClassAttributeAst*>;

  /// Check that all the instances of a "cmp" constant, in a
  /// type and its super types it is extending, have a consistent
  /// type.
  SPP_EXP_FUN auto CheckShadowedCmpAgreesInType(
    CmpStatementAst const &cmp_member, Scope &cls_scope,
    Scope const &own_scope, ScopeManager const &sm) -> void;

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
    Scope const &type_scope) -> Vec<FunctionPrototypeAst const*>;

  /// Whether the abstract method "abs_fn", unimplemented on the
  /// type, is left so by an error already reported: a recovering
  /// compile skips an extension block that failed, so a method it
  /// declares to implement "abs_fn" is missing because of that
  /// error, and a use of the type is not a second one.
  SPP_EXP_FUN auto IsLeftUnimplementedByAnError(
    Scope const &type_scope, FunctionPrototypeAst const &abs_fn) -> bool;

  /// Get the index of a field on its type, so that LLVM
  /// can determine which slot to push data into. Takes into
  /// account hidden fat pointer fields too.
  SPP_EXP_FUN auto GetFieldIndexInType(
    TypeSymbol const &type_sym, IdentifierAst const &field_name) -> std::size_t;

  /// The classes "ref" is superimposed as ("sup Foo ext Bar"),
  /// in the order its sup scopes list them, each read where
  /// "scope" reads it. Empty for a type with no scope of its own
  /// (a generic parameter).
  SPP_EXP_FUN auto SuperClsRefs(
    TypeRef const &ref, Scope const &scope) -> Vec<TypeRef>;

  /// The sup scopes among "sup_scopes" that superimpose a class
  /// ("sup Foo ext Bar"), in order. One is keyed where its own sup
  /// scope reads it ("TypeIdOfSymbol" of its linked symbol there): a
  /// sup type's name can hold a "Self" (as in "S32 ext
  /// Ord[Rhs=Self]") only that scope has a symbol for.
  SPP_EXP_FUN auto SuperClsScopes(
    Vec<Scope*> const &sup_scopes) -> Vec<Scope const*>;

  /// Get the number of synthetic fat-pointer fields on this
  /// type, typically the resume_fn/env_ptr or fn_ptr/env_ptr
  /// fields prepended ahead of a type's own declared fields.
  /// The fat pointer fields are always at the start of the
  /// types for simplicity.
  SPP_EXP_FUN auto GetSuperimposedFatPointerFieldCount(
    TypeSymbol const &type_sym) -> std::size_t;

  /// The first type a value of @p ref holds by value (itself
  /// included) that @p matches: as a tuple or array element, a
  /// variant member, or an attribute, at any depth. A borrow or
  /// a pointer holds nothing by value ("Vec[T]" keeps its "T"s
  /// behind "RawBuf"'s pointer). No type when nothing matches.
  SPP_EXP_FUN auto FindHeldByValue(
    TypeRef const &ref, Scope const &scope,
    FuncRef<bool(TypeRef const &)> matches) -> TypeRef;

  /// The written type of the first attribute that holds the type
  /// itself by value, directly or through anything it holds by
  /// value, making the type infinitely large; null if none does.
  SPP_EXP_FUN auto FindRecursiveAttrType(
    ClassPrototypeAst const &type, ScopeManager const &sm) -> Shared<TypeAst>;

  /// The number of elements an index can reach in a tuple or an
  /// array: a tuple's arguments, an array's compile-time "n". An
  /// index below it is in bounds. The type must be one of them.
  SPP_EXP_FUN auto IndexBoundOf(
    TypeRef const &ref, Scope const &scope) -> std::size_t;

  /// Get the nth type of a tuple, or for an array, all the types
  /// are the same: read off the instance's identity, where its
  /// scope reads it.
  SPP_EXP_FUN auto GetNthTypeOfIndexableType(
    std::size_t index, TypeRef const &ref, Scope const &scope) -> TypeRef;

  /// A variant's members in their one canonical order ("Bool or S32"
  /// is "S32 or Bool"): by the qualified name of what each names (the
  /// symbol given with it), else by its spelling, ties kept in order.
  /// The analysed variant ("TypeIdentifierAst::Stage7") and the one
  /// built from an identity ("Scope::TypeAstOf") are both put in it,
  /// so their members' tags agree.
  SPP_EXP_FUN auto OrderVariantMembers(
    Vec<Pair<Shared<TypeAst>, TypeSymbol const*>> members)
    -> Vec<Shared<TypeAst>>;

  /// A variant's members as types, flattened through nested
  /// variants and without duplicates: "Str or Str or S32" is
  /// "Str, S32". Members written directly keep their spelling.
  SPP_EXP_FUN auto VariantMembers(
    TypeAst const &type, Scope const &scope) -> Vec<Shared<TypeAst>>;

  /// A variant's members as resolved types: flattened through nested
  /// variants and without duplicates, none for a non-variant. What a
  /// reader that needs the members' symbols, not their names, asks.
  SPP_EXP_FUN auto VariantMemberRefs(
    TypeRef const &ref, Scope const &scope) -> Vec<TypeRef>;
}
