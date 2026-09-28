module;
#include <spp/macros.hpp>

export module spp.analyse.utils.arg_naming;
import spp.asts.meta.compiler_meta_data;
import spp.utils.ptr;
import spp.utils.types;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::asts, struct Ast);
use(spp::asts, struct FunctionCallArgumentGroupAst);
use(spp::asts, struct FunctionParameterGroupAst);
use(spp::asts, struct GenericArgumentAst);
use(spp::asts, struct GenericArgumentGroupAst);
use(spp::asts, struct GenericParameterGroupAst);

namespace spp::analyse::utils::arg_naming {
  /// Take a generic argument group, and name the arguments
  /// based on the parameters available. Custom logic for
  /// optional and variadic parameters.
  SPP_EXP_FUN auto NameGnArgs(
    GenericArgumentGroupAst &a_group,
    GenericParameterGroupAst const &p_group,
    Ast const &owner,
    ScopeManager &sm,
    meta::CompilerMetaData &meta,
    bool is_tuple_owner = false)
    -> void;

  /// Take a function argument group, and name the arguments
  /// based on the parameters available. Custom logic for
  /// optional and variadic parameters.
  SPP_EXP_FUN auto NameFnArgs(
    FunctionCallArgumentGroupAst &a_group,
    FunctionParameterGroupAst const &p_group,
    ScopeManager &sm,
    meta::CompilerMetaData *meta,
    Vec<GenericArgumentAst*> const &generic_args = {},
    Scope *callee_scope = nullptr)
    -> void;

  /// Reject a keyword argument naming none of "param_names", the
  /// parameters of "params" that the call can name.
  SPP_EXP_FUN auto EnforceFnArgNamesKnown(
    Vec<Shared<IdentifierAst>> const &param_names,
    Vec<IdentifierAst*> const &arg_names,
    FunctionParameterGroupAst const &params,
    ScopeManager const &sm)
    -> void;

}
