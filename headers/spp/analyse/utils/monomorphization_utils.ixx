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

namespace spp::codegen {
  SPP_EXP_CLS struct LlvmCtx;
}

namespace spp::analyse::utils::monomorphization_utils {
  /**
   * Analyse the body of every generic instantiation in the program, to a fixed point.
   *
   * @n
   * Instantiations are discovered by analysing calls, and analysing an instantiation's body is itself what discovers
   * the instantiations that body calls for. The set is therefore only closed once analysing everything in it produces
   * nothing new - which is what this does, and why it cannot be a walk over the modules: a walk visits each module
   * once, in an order nothing guarantees, and "std::intrinsics::add[T=S32]" is only reached by analysing the body of
   * "SizedInteger[32, true]::add", which is only reached by analysing a call written in a third module entirely.
   *
   * @param[in] sm The scope manager, reset to the global scope on return.
   * @param[in] meta The compiler meta data.
   */
  SPP_EXP_FUN auto MonomorphiseToFixedPoint(
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> void;

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
   * @param old_fun_scope The mock "sup" block Stage1 lowered the function into, not the function's own scope - the
   * prototype sits one level below, and its parameters and locals below that. The whole subtree is cloned.
   * @param generic_args The arguments the generic parameters are being bound to.
   * @param external_generic_syms Generic symbols reachable from the scope the call was made in.
   * @param sm The scope manager.
   * @param meta The compiler meta data.
   * @return The scope created for the instantiation, at the same level as the one handed in - see
   * @c FunctionPrototypeAst::GenericSubstitution::WalkScope , which is where it ends up.
   */
  SPP_EXP_FUN auto CreateGenericFunScope(
    scopes::Scope const &old_fun_scope,
    asts::GenericArgumentGroupAst const &generic_args,
    SharedVec<scopes::Symbol> const &external_generic_syms,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> scopes::Scope*;

  /**
   * Drop every generic "sup" block instantiation remembered by @c CreateGenericSupScope . The cache names scopes by
   * address, so it must not outlive them.
   */
  SPP_EXP_FUN auto ClearSupScopeInstantiations() -> void;

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
