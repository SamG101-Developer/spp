module;
#include <spp/macros.hpp>

export module spp.analyse.utils.overload_utils;
import spp.analyse.utils.func_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.types;
import std;

namespace spp::analyse::errors {
  SPP_EXP_CLS struct SemanticError;
}

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts {
  SPP_EXP_CLS struct FunctionCallArgumentGroupAst;
  SPP_EXP_CLS struct FunctionParameterGroupAst;
  SPP_EXP_CLS struct FunctionPrototypeAst;
  SPP_EXP_CLS struct GenericArgumentAst;
  SPP_EXP_CLS struct GenericArgumentGroupAst;
  SPP_EXP_CLS struct GenericParameterGroupAst;
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct PostfixExpressionAst;
  SPP_EXP_CLS struct PostfixExpressionOperatorFunctionCallAst;
  SPP_EXP_CLS struct TypeAst;
}

namespace spp::analyse::utils::overload_utils {
  SPP_EXP_CLS struct PassedOverload {
    scopes::Scope const *FnScope;
    asts::FunctionPrototypeAst *Proto;
    Unique<asts::FunctionCallArgumentGroupAst> FnArgs;
  };

  SPP_EXP_CLS struct FailedOverload {
    asts::FunctionPrototypeAst *Proto;
    Str Error;
    Str Reason;
  };

  SPP_EXP_CLS struct OverloadCandidates {
    bool IsClosure;
    Unique<asts::FunctionPrototypeAst> ClosureProto;
    Vec<func_utils::FunctionOverload> Overloads;
  };

  SPP_EXP_CLS struct PropagatedMethodCall {
    PassedOverload Overload;
    bool IsClosure;
    Unique<asts::PostfixExpressionAst> TransformedAst;
  };

  SPP_EXP_FUN auto DetermineOverload(
    asts::PostfixExpressionOperatorFunctionCallAst &fn_call,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> Pair<PassedOverload, bool>;

  SPP_EXP_FUN auto PropagateMethodToFunction(
    asts::PostfixExpressionOperatorFunctionCallAst &fn_call,
    asts::TypeAst const &fn_owner_type,
    asts::IdentifierAst const &fn_name,
    asts::PostfixExpressionAst const &cast_lhs,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> PropagatedMethodCall;

  SPP_EXP_FUN auto RetrieveAllOverloads(
    asts::IdentifierAst const *fn_name,
    scopes::Scope const &fn_owner_scope,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> OverloadCandidates;

  SPP_EXP_FUN auto RetrieveOwnerGenericArgs(
    Shared<asts::TypeAst> const &fwd_type,
    asts::meta::CompilerMetaData const *meta)
    -> Vec<Unique<asts::GenericArgumentAst>>;

  SPP_EXP_FUN auto InferAllGenerics(
    asts::FunctionPrototypeAst const &fn_proto,
    asts::FunctionParameterGroupAst const &fn_params,
    asts::FunctionCallArgumentGroupAst &fn_args,
    asts::GenericArgumentGroupAst &gn_args,
    bool is_variadic_fn,
    scopes::Scope const *fn_scope,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> void;

  SPP_EXP_FUN auto PotentiallyGenerateGenericSubstitutedPrototype(
    asts::FunctionPrototypeAst *fn_proto,
    scopes::Scope const *fn_scope,
    asts::GenericArgumentGroupAst &generic_args,
    Shared<asts::TypeAst> const &variadic_pack_type,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> Tup<asts::FunctionPrototypeAst*, scopes::Scope const*>;

  SPP_EXP_FUN auto ValidateArgsMatchParams(
    asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
    asts::FunctionPrototypeAst const &fn_proto,
    scopes::Scope const *fn_scope,
    asts::FunctionCallArgumentGroupAst const &func_args,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> void;

  SPP_EXP_FUN auto ManageMatchedOverloads(
    asts::PostfixExpressionOperatorFunctionCallAst const &fn_call,
    Vec<PassedOverload> const &pass_overloads,
    Vec<FailedOverload> const &fail_overloads,
    asts::FunctionCallArgumentGroupAst const &arg_group,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> void;
}
