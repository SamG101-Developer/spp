module;
#include <spp/macros.hpp>

export module spp.analyse.utils.monomorphization_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct GenericArgumentAst;
  SPP_EXP_CLS struct GenericArgumentGroupAst;
  SPP_EXP_CLS struct TypeIdentifierAst;
}

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS class ScopeManager;
  SPP_EXP_CLS struct Symbol;
  SPP_EXP_CLS struct TypeSymbol;
}

/**
 * Monomorphization: turning a generic template into a concrete instantiation. Every generic construct - a class, a
 * "sup" block, a function - is instantiated by cloning the template's scope, giving the clone its own symbols and its
 * own copy of the template's ast, and binding the generic parameters to the arguments the instantiation was created
 * for. These live apart from @c type_utils because they are the only place that creates scopes, rather than answering
 * questions about types that already exist.
 */
namespace spp::analyse::utils::monomorphization_utils {
  /**
   * Create the scope for a generic substitution of a class, and register it against the class it instantiates.
   * @param type_part The type naming the instantiation, whose generic arguments are the ones being bound.
   * @param old_cls_sym The symbol of the class being instantiated.
   * @param external_generic_syms Generic symbols reachable from the scope the instantiation was named in.
   * @param is_tuple Whether the class is a tuple, which skips the attribute substitution.
   * @param sm The scope manager.
   * @param meta The compiler meta data.
   * @return The scope created for the instantiation.
   */
  SPP_EXP_FUN auto CreateGenericClsScope(
    asts::TypeIdentifierAst &type_part,
    Shared<scopes::TypeSymbol> const &old_cls_sym,
    SharedVec<scopes::Symbol> const &external_generic_syms,
    bool is_tuple,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> scopes::Scope*;

  /**
   * Create the scope for a generic substitution of a function, and register it against the prototype it instantiates.
   * @param old_fun_scope The scope of the function being instantiated.
   * @param generic_args The arguments the generic parameters are being bound to.
   * @param external_generic_syms Generic symbols reachable from the scope the call was made in.
   * @param sm The scope manager.
   * @param meta The compiler meta data.
   * @return The scope created for the instantiation.
   */
  SPP_EXP_FUN auto CreateGenericFunScope(
    scopes::Scope const &old_fun_scope,
    asts::GenericArgumentGroupAst const &generic_args,
    SharedVec<scopes::Symbol> const &external_generic_syms,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> scopes::Scope*;

  /**
   * Create the scope for a generic substitution of a "sup" block over an instantiated class.
   * @param old_sup_scope The scope of the "sup" block being instantiated.
   * @param new_cls_scope The scope of the class instantiation the block is being superimposed over.
   * @param generic_args The arguments the generic parameters are being bound to.
   * @param external_generic_syms Generic symbols reachable from the scope the instantiation was named in.
   * @param sm The scope manager.
   * @param meta The compiler meta data.
   * @return The scope created for the instantiation, and the scope of its substituted super class if it extends one.
   */
  SPP_EXP_FUN auto CreateGenericSupScope(
    scopes::Scope &old_sup_scope,
    scopes::Scope &new_cls_scope,
    asts::GenericArgumentGroupAst const &generic_args,
    SharedVec<scopes::Symbol> const &external_generic_syms,
    scopes::ScopeManager const *sm,
    asts::meta::CompilerMetaData *meta)
    -> std::tuple<scopes::Scope*, scopes::Scope*>;

  /**
   * Create the symbol that binds one generic parameter to the argument given for it: a type symbol naming the bound
   * type, or a variable symbol carrying the bound comp-time value.
   * @param generic The generic argument being bound.
   * @param sm The scope manager.
   * @param meta The compiler meta data.
   * @param tm An alternative scope manager to resolve the argument through, if it is not resolved through @p sm.
   * @return The symbol for the binding.
   */
  SPP_EXP_FUN auto CreateGenericSym(
    asts::GenericArgumentAst const &generic,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta,
    scopes::ScopeManager *tm = nullptr)
    -> Shared<scopes::Symbol>;

  /**
   * Register the bindings for a set of generic arguments, and the generic symbols carried in from the scope the
   * instantiation was named in, into an instantiation's scope.
   * @param external_generic_syms Generic symbols reachable from the scope the instantiation was named in.
   * @param generic_args The arguments the generic parameters are being bound to.
   * @param scope The instantiation's scope to register the symbols into.
   * @param sm The scope manager.
   * @param meta The compiler meta data.
   */
  SPP_EXP_FUN auto RegisterGenericSyms(
    SharedVec<scopes::Symbol> const &external_generic_syms,
    UniqueVec<asts::GenericArgumentAst> const &generic_args,
    scopes::Scope *scope,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> void;

  /**
   * Rewrite a "sup" block's scope name for an instantiation, so that the substituted block is named after the
   * arguments it was created for rather than the template's parameters.
   * @param old_sup_scope_name The template block's scope name.
   * @param generic_args The arguments the generic parameters are being bound to.
   * @return The instantiation's scope name.
   */
  SPP_EXP_FUN auto SubstituteSupScopeName(
    Str const &old_sup_scope_name,
    asts::GenericArgumentGroupAst const &generic_args)
    -> Str;
}
