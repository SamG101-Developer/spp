module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_optional_ast;
import spp.asts.ast_kind;
import spp.asts.function_parameter_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(FunctionParameterOptionalAst);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct TokenAst);

/// An optional parameter in a function prototype, which has a
/// default value and can be omitted when calling the function.
SPP_EXP_CLS struct spp::asts::FunctionParameterOptionalAst final : FunctionParameterAst {
  SPP_AST_KEY_FUNCTIONS(FunctionParameterOptionalAst);

  /// The "=" token separating the parameter from its default.
  Unique<TokenAst> TokAssign;

  /// The default value, used if the argument is not provided.
  Unique<ExpressionAst> DefaultVal;

  FunctionParameterOptionalAst(
    decltype(Var) &&var,
    decltype(TokColon) &&tok_colon,
    decltype(Type) type,
    decltype(TokAssign) &&tok_assign,
    decltype(DefaultVal) &&default_val);

  ~FunctionParameterOptionalAst() override;

  auto Stage6_PreAnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

private:
  bool _DefaultAnalysed = false;
};
