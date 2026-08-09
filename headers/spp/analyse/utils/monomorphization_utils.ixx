module;
#include <spp/macros.hpp>

export module spp.analyse.utils.monomorphization_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct GenericArgumentGroupAst;
  SPP_EXP_CLS struct TypeIdentifierAst;
}

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS class ScopeManager;
  SPP_EXP_CLS struct Symbol;
  SPP_EXP_CLS struct TypeSymbol;
}

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
   * The substituted prototype itself is filled in by the caller, which has the parameter and return type substitution
   * to do; this reserves the slot it goes in.
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
    -> Tup<scopes::Scope*, scopes::Scope*>;
}
