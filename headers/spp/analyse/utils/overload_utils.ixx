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

  SPP_EXP_FUN auto PotentiallyGenerateGenericSubstitutedPrototype(
    asts::FunctionPrototypeAst *fn_proto,
    scopes::Scope const *fn_scope,
    asts::GenericArgumentGroupAst &generic_args,
    Shared<asts::TypeAst> const &variadic_pack_type,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> Tup<asts::FunctionPrototypeAst*, scopes::Scope const*>;

}
