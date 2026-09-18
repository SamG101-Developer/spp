module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_variadic_ast;
import spp.asts.ast_kind;
import spp.asts.function_parameter_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(FunctionParameterVariadicAst);
use(spp::asts, struct TokenAst);

/// A variadic parameter in a function prototype, which can
/// accept an arbitrary number of arguments, like "*args" in
/// Python.
SPP_EXP_CLS struct spp::asts::FunctionParameterVariadicAst final : FunctionParameterAst {
  SPP_AST_KEY_FUNCTIONS(FunctionParameterVariadicAst);

  /// The ".." token marking the parameter as variadic.
  Unique<TokenAst> TokEllipsis;

  FunctionParameterVariadicAst(
    decltype(TokEllipsis) &&tok_ellipsis,
    decltype(Var) &&var,
    decltype(TokColon) &&tok_colon,
    decltype(Type) type);

  ~FunctionParameterVariadicAst() override;
};
