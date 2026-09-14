module;
#include <spp/macros.hpp>

export module spp.asts.function_call_argument_keyword_ast;
import spp.asts.ast_kind;
import spp.asts.function_call_argument_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(FunctionCallArgumentKeywordAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TokenAst);

/// A keyword argument in a function call, which forces the
/// argument to be matched by keyword rather than by index.
SPP_EXP_CLS struct spp::asts::FunctionCallArgumentKeywordAst final : FunctionCallArgumentAst {
  SPP_AST_KEY_FUNCTIONS(FunctionCallArgumentKeywordAst);

  /// The name used to refer to the argument in the function
  /// call.
  Shared<IdentifierAst> Name;

  /// The "=" token, separating the name of the argument from
  /// the expression being passed as its value.
  Unique<TokenAst> TokAssign;

  FunctionCallArgumentKeywordAst(
    decltype(Name) name,
    decltype(TokAssign) &&tok_assign,
    decltype(Conv) &&conv,
    decltype(Val) &&val);

  ~FunctionCallArgumentKeywordAst() override;
};
