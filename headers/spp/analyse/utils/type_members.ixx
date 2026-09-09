module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_members;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct ClassAttributeAst;
  SPP_EXP_CLS struct CmpStatementAst;
  SPP_EXP_CLS struct FunctionPrototypeAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct TypeAst;
}

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS class ScopeManager;
  SPP_EXP_CLS struct TypeSymbol;
}

namespace spp::analyse::utils::type_members {
  /**
   * One part of a type: an attribute, or an element of a tuple or an array. The two are held differently - attributes
   * by name, elements by position - and every caller that walks what a type is made of has to cope with both, so the
   * difference is settled here once rather than at each of them.
   */
  SPP_EXP_CLS struct TypePart {
    Str Step; // The attribute's name, or the element's index written out
    std::size_t Index; // Position in the walk, which for a tuple or an array is the element's index.
    Shared<asts::TypeAst> Type; // The part's own type.
    scopes::TypeSymbol *Sym; // The symbol of the part's own type.
    scopes::Scope const *Where; // The scope that the part's type resolves in.
  };

  /**
   * The parts a type is made of, in declaration order: its elements when it is a tuple or an array, its attributes
   * (and its super types') otherwise.
   * @param type The type being taken apart.
   * @param scope A scope to look @p type up from.
   * @param collapse_arrays Answer with only the first element of an array. Every element of an array has the same
   * type, so one of them answers for all of them wherever the question is about the type rather than the storage.
   * @return Its parts, empty when it has none.
   */
  SPP_EXP_FUN auto GetAllParts(
    asts::TypeAst const &type,
    scopes::Scope const &scope,
    bool collapse_arrays = false)
    -> Vec<TypePart>;

  /**
   * The attributes of a type and all of its super types, each with the symbol of its own type and the scope that type
   * resolves in. All this needs is a scope to look @p type up from.
   */
  SPP_EXP_FUN auto GetAllAttrs(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> Vec<Tup<Shared<asts::IdentifierAst>, scopes::TypeSymbol*, scopes::Scope*>>;

  /**
   * Check all instances of the constant in the scope and super scopes have the same type.
   * @param cmp_member The constant being declared.
   * @param cls_scope The scope of the type the superimposition is over.
   * @param own_scope The scope of the superimposition block declaring it, whose own declaration is not compared
   * against itself.
   * @param sm The scope manager, for the scope errors are reported against.
   */
  SPP_EXP_FUN auto CheckShadowedCmpAgreesInType(
    asts::CmpStatementAst const &cmp_member,
    scopes::Scope &cls_scope,
    scopes::Scope const &own_scope,
    scopes::ScopeManager const &sm)
    -> void;

  /**
   * Drop everything @c GetUnimplementedAbstractMethods has remembered. Its cache is keyed on scope addresses, so it
   * must not outlive the scopes it names - a later run allocating a scope at a freed address would otherwise be
   * handed the old scope's answer.
   */
  SPP_EXP_FUN auto ClearUnimplementedAbstractMethodsCache()
    -> void;

  /**
   * Collect the methods that are visible on a type but left unimplemented, that is, the methods declared with the
   * @c !abstract_method annotation that no superimposition on the type provides a same-signature implementation for.
   * A type with any such method is abstract: it cannot be instantiated, because calling one of them would have no
   * body to dispatch to. Abstractness propagates, so a type that superimposes an abstract type and implements only
   * some of its abstract methods is itself abstract, and reports the ones that are still outstanding.
   * @param type_scope The scope of the type whose methods are being collected.
   * @return The abstract methods that the type never implements, empty if the type is concrete.
   */
  SPP_EXP_FUN auto GetUnimplementedAbstractMethods(
    scopes::Scope const &type_scope)
    -> Vec<asts::FunctionPrototypeAst const*>;

  /**
   * Get the class attribute ASTs of a type and all of its super types, in the same order as @c GetAllAttrs, so the
   * two line up index for index. This gives access to per-attribute information that isn't carried on the variable
   * symbols, such as an attribute's default value.
   * @param type The type whose attribute ASTs are being collected.
   * @param sm The scope manager, used to resolve the type.
   * @return The attribute ASTs of the type and its super types.
   */
  SPP_EXP_FUN auto GetAllAttrAsts(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> Vec<asts::ClassAttributeAst*>;

  SPP_EXP_FUN auto GetFieldIndexInType(
    asts::TypeAst const &type_sym,
    asts::IdentifierAst const &field_name,
    scopes::Scope const &scope)
    -> std::size_t;
}
