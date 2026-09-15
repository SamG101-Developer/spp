module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_members;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::utils::type_members, struct TypePart);
use(spp::asts, struct ClassAttributeAst);
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
    TypeAst const &type,
    Scope const &scope,
    bool collapse_arrays = false)
    -> Vec<TypePart>;

  /// Get all the fields on a type, and all of it's super types,
  /// tracking the field, symbol, and scope. The scope is so
  /// we know which super class it came from if it's not on the
  /// actual type itself.
  SPP_EXP_FUN auto GetAllAttrs(
    TypeAst const &type,
    Scope const &scope)
    -> Vec<Tup<Shared<IdentifierAst>, TypeSymbol*, Scope*>>;

  /// Similar to the "GetAllAttrs", but in ast form, so that
  /// the default values can be extracted for object initializers,
  /// if required.
  SPP_EXP_FUN auto GetAllAttrAsts(
    TypeAst const &type,
    Scope const &scope)
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
    TypeAst const &type_sym,
    IdentifierAst const &field_name,
    Scope const &scope)
    -> std::size_t;
}
