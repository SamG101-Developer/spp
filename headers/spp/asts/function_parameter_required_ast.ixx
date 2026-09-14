module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_required_ast;
import spp.asts.ast_kind;
import spp.asts.function_parameter_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(FunctionParameterRequiredAst);

SPP_EXP_CLS struct spp::asts::FunctionParameterRequiredAst final : FunctionParameterAst {
  SPP_AST_KEY_FUNCTIONS(FunctionParameterRequiredAst);

  FunctionParameterRequiredAst(
    decltype(Var) &&var,
    decltype(TokColon) &&tok_colon,
    decltype(Type) type);

  ~FunctionParameterRequiredAst() override;
};
