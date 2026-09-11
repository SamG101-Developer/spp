module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_self_ast;
import spp.asts.ast_kind;
import spp.asts.function_parameter_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(FunctionParameterSelfAst) {
  SPP_EXP_CLS struct ConventionAst;
}

SPP_EXP_CLS struct spp::asts::FunctionParameterSelfAst final : FunctionParameterAst {
  SPP_AST_KEY_FUNCTIONS(FunctionParameterSelfAst);

  /**
   * The convention is attached to the self parameter rather than its type, as it is required before the type is
   * necessarily attached.
   */
  Unique<ConventionAst> Conv;

  /**
   * Construct the FunctionParameterSelfAst with the arguments matching the members.
   * @param conv The convention token for this parameter.
   * @param var The local variable declaration for this parameter.
   */
  FunctionParameterSelfAst(
    decltype(Conv) &&conv,
    decltype(Var) &&var);

  ~FunctionParameterSelfAst() override;

  auto Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;
};
