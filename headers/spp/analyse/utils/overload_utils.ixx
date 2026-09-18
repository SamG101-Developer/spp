module;
#include <spp/macros.hpp>

export module spp.analyse.utils.overload_utils;
import spp.asts.meta.compiler_meta_data;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::asts, struct FunctionCallArgumentGroupAst);
use(spp::asts, struct FunctionPrototypeAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct PostfixExpressionOperatorFunctionCallAst);

namespace spp::analyse::utils::overload_utils {
  SPP_EXP_CLS struct PassedOverload {
    Scope const *FnScope;
    FunctionPrototypeAst *Proto;
    Unique<FunctionCallArgumentGroupAst> FnArgs;
  };

  SPP_EXP_CLS struct FailedOverload {
    FunctionPrototypeAst *Proto;
    Str Error;
    Str Reason;
  };

  /// Given a function call, perform a complex set of steps
  /// to determine the overload being used. This requires
  /// method-to-function propagation, heavy generic inference,
  /// type forwarding, argument validation, and candidate
  /// disambiguation.
  SPP_EXP_FUN auto DetermineOverload(
    PostfixExpressionOperatorFunctionCallAst &fn_call,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> Pair<PassedOverload, bool>;

  /// Instantiate a generic substitution of a function, creating
  /// the new body etc for analysis. Typically called from the
  /// func_utils wrapper.
  SPP_EXP_FUN auto InstantiateOverload(
    FunctionPrototypeAst *fn_proto,
    Scope const *fn_scope,
    GenericArgumentGroupAst &generic_args,
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> FunctionPrototypeAst*;

  /// Find an instantiated overload, given a function prototype
  /// and a generic argument group. Typically called from the
  /// func_utils wrapper.
  SPP_EXP_FUN auto FindInstantiatedOverload(
    FunctionPrototypeAst *fn_proto,
    GenericArgumentGroupAst &generic_args,
    ScopeManager const *sm)
    -> FunctionPrototypeAst*;
}
