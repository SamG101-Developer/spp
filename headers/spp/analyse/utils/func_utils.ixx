module;
#include <spp/macros.hpp>

export module spp.analyse.utils.func_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct Ast;
  SPP_EXP_CLS struct ExpressionAst;
  SPP_EXP_CLS struct FunctionCallArgumentAst;
  SPP_EXP_CLS struct FunctionCallArgumentKeywordAst;
  SPP_EXP_CLS struct FunctionCallArgumentGroupAst;
  SPP_EXP_CLS struct FunctionParameterAst;
  SPP_EXP_CLS struct FunctionParameterGroupAst;
  SPP_EXP_CLS struct FunctionPrototypeAst;
  SPP_EXP_CLS struct GenericArgumentAst;
  SPP_EXP_CLS struct GenericArgumentCompAst;
  SPP_EXP_CLS struct GenericArgumentCompKeywordAst;
  SPP_EXP_CLS struct GenericArgumentTypeAst;
  SPP_EXP_CLS struct GenericArgumentTypeKeywordAst;
  SPP_EXP_CLS struct GenericArgumentGroupAst;
  SPP_EXP_CLS struct GenericParameterAst;
  SPP_EXP_CLS struct GenericParameterCompAst;
  SPP_EXP_CLS struct GenericParameterGroupAst;
  SPP_EXP_CLS struct GenericParameterTypeAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct PostfixExpressionAst;
  SPP_EXP_CLS struct PostfixExpressionOperatorFunctionCallAst;
  SPP_EXP_CLS struct TypeAst;
  SPP_EXP_CLS struct TypeIdentifierAst;
}

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS class ScopeManager;
}

namespace spp::analyse::utils::func_utils {

  SPP_EXP_CLS struct FunctionOverload {
    scopes::Scope const *FnScope;
    asts::FunctionPrototypeAst *Proto;
    Unique<asts::GenericArgumentGroupAst> SupGenerics;
    Shared<asts::TypeAst> FwdType;
  };



  SPP_EXP_FUN auto GetAllFunctionScopes(
    asts::IdentifierAst const &target_fn_name,
    scopes::Scope const *target_scope,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> Vec<FunctionOverload>;

  SPP_EXP_FUN auto CheckForConflictingOverload(
    scopes::Scope const &this_scope,
    scopes::Scope const *target_scope,
    asts::FunctionPrototypeAst const &new_fn,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> asts::FunctionPrototypeAst*;

  SPP_EXP_FUN auto SameSignature(
    asts::FunctionPrototypeAst const &fn_a,
    scopes::Scope const &scope_a,
    asts::FunctionPrototypeAst const &fn_b,
    scopes::Scope const &scope_b)
    -> bool;

  SPP_EXP_FUN auto CheckForConflictingOverride(
    scopes::Scope const &this_scope,
    scopes::Scope const *target_scope,
    asts::FunctionPrototypeAst const &new_fn,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta,
    scopes::Scope const *exclude_scope = nullptr)
    -> asts::FunctionPrototypeAst*;

  /**
   * @param generic_args The generic bindings known for this call, used to translate the default value of any optional
   * parameter the call left out. Such a default is the callee's own expression and is materialised into the caller's
   * argument list, so a default like @c "alloc: A = A()" arrives at the call site still naming @c "A" - a name only
   * the callee has - unless it is rewritten as it is materialised.
   */
  SPP_EXP_FUN auto NameFnArgs(
    asts::FunctionCallArgumentGroupAst &a_group,
    asts::FunctionParameterGroupAst const &p_group,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta,
    Vec<asts::GenericArgumentAst*> const &generic_args = {},
    scopes::Scope *callee_scope = nullptr)
    -> void;

  SPP_EXP_FUN auto IsTargetCallable(
    asts::ExpressionAst &expr,
    scopes::ScopeManager &sm,
    asts::meta::CompilerMetaData *meta)
    -> Shared<const asts::TypeAst>;

}
