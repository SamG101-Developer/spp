module;
#include <spp/macros.hpp>

export module spp.asts.function_call_argument_positional_ast;
import spp.asts.ast_kind;
import spp.asts.function_call_argument_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(FunctionCallArgumentPositionalAst);
use(spp::asts, struct TokenAst);

/// A positional argument in a function call, which forces the
/// argument to be matched by index rather than by keyword. It
/// also supports unpacking a tuple into arguments.
SPP_EXP_CLS struct spp::asts::FunctionCallArgumentPositionalAst : FunctionCallArgumentAst {
  SPP_AST_KEY_FUNCTIONS(FunctionCallArgumentPositionalAst);

  /// The ".." unpacking token, showing the argument is a tuple
  /// being unpacked into the resulting arguments.
  Unique<TokenAst> TokUnpack;

  FunctionCallArgumentPositionalAst(
    decltype(Conv) &&conv,
    decltype(TokUnpack) &&tok_unpack,
    decltype(Val) &&val);

  ~FunctionCallArgumentPositionalAst() override;
};
