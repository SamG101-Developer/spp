module;
#include <spp/macros.hpp>

export module spp.analyse.utils.overload_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.types;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts {
  SPP_EXP_CLS struct FunctionCallArgumentGroupAst;
  SPP_EXP_CLS struct FunctionPrototypeAst;
  SPP_EXP_CLS struct GenericArgumentGroupAst;
  SPP_EXP_CLS struct PostfixExpressionOperatorFunctionCallAst;
}

namespace spp::analyse::utils::overload_utils {
  SPP_EXP_CLS struct PassedOverload {
    scopes::Scope const *FnScope;
    asts::FunctionPrototypeAst *Proto;
    Unique<asts::FunctionCallArgumentGroupAst> FnArgs;
  };

  SPP_EXP_FUN auto DetermineOverload(
    asts::PostfixExpressionOperatorFunctionCallAst &fn_call,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> Pair<PassedOverload, bool>;

  SPP_EXP_FUN auto InstantiateOverload(
    asts::FunctionPrototypeAst *fn_proto,
    scopes::Scope const *fn_scope,
    asts::GenericArgumentGroupAst &generic_args,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> asts::FunctionPrototypeAst*;

  SPP_EXP_FUN auto FindInstantiatedOverload(
    asts::FunctionPrototypeAst *fn_proto,
    asts::GenericArgumentGroupAst &generic_args,
    scopes::ScopeManager const *sm)
    -> asts::FunctionPrototypeAst*;
}
