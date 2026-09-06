module;
#include <spp/macros.hpp>

export module spp.analyse.utils.type_members;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct ClassAttributeAst;
  SPP_EXP_CLS struct FunctionPrototypeAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct TypeAst;
}

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS struct TypeSymbol;
}

namespace spp::analyse::utils::type_members {
  /**
   * The attributes of a type and all of its super types, each with the symbol of its own type and the scope that type
   * resolves in. All this needs is a scope to look @p type up from.
   */
  SPP_EXP_FUN auto GetAllAttrs(
    asts::TypeAst const &type,
    scopes::Scope const &scope)
    -> Vec<Tup<Shared<asts::IdentifierAst>, scopes::TypeSymbol*, scopes::Scope*>>;

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
