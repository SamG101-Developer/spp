module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_optional_ast;
import spp.asts.ast_kind;
import spp.asts.function_parameter_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(FunctionParameterOptionalAst) {
  SPP_EXP_CLS struct ExpressionAst;
  SPP_EXP_CLS struct TokenAst;
}

/**
 * The FunctionParameterOptionalAst represents an optional parameter in a function prototype. It is used to define
 * parameters that are not required, and can be omitted when calling the function.
 */
SPP_EXP_CLS struct spp::asts::FunctionParameterOptionalAst final : FunctionParameterAst {
  SPP_AST_KEY_FUNCTIONS(FunctionParameterOptionalAst);

  /**
   * The token that separates the parameter name from the default value.
   */
  Unique<TokenAst> TokAssign;

  /**
   * The default value for the parameter. This is the expression that will be used if the parameter is not provided.
   */
  Unique<ExpressionAst> DefaultVal;

  /**
   * Construct the FunctionParameterOptionalAst with the arguments matching the members.
   * @param var The local variable declaration for this parameter.
   * @param tok_colon The token that represents the @c : colon in the function parameter.
   * @param type The type of the parameter.
   * @param tok_assign The token that separates the parameter name from the default value.
   * @param default_val The default value for the parameter.
   */
  FunctionParameterOptionalAst(
    decltype(Var) &&var,
    decltype(TokColon) &&tok_colon,
    decltype(Type) type,
    decltype(TokAssign) &&tok_assign,
    decltype(DefaultVal) &&default_val);

  ~FunctionParameterOptionalAst() override;

  auto Stage6_PreAnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

private:
  bool _DefaultAnalysed = false;
};
