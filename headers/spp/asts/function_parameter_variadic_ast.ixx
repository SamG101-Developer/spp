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

  /// Whether the pack is over a type pack ("..xs: Ts"), which binds
  /// the list of its elements' types; otherwise each element is of
  /// one type ("..xs: T", "..xs: S32"), and a hidden pack stands for
  /// its length. Decided where the template's parameter is analysed
  /// ("FunctionParameterAst::Stage7"); an instance's copy carries it.
  bool OverTypePack = false;

  FunctionParameterVariadicAst(
    decltype(TokEllipsis) &&tok_ellipsis,
    decltype(Var) &&var,
    decltype(TokColon) &&tok_colon,
    decltype(Type) type);

  ~FunctionParameterVariadicAst() override;
};
