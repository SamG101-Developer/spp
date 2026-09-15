module;
#include <spp/macros.hpp>

export module spp.asts.object_initializer_argument_keyword_ast;
import spp.asts.ast_kind;
import spp.asts.object_initializer_argument_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(ObjectInitializerArgumentKeywordAst);
use(spp::asts, struct TokenAst);

/// A keyword argument in an object initializer. It forces the
/// argument to be matched by a keyword rather than a
/// shorthand value.
SPP_EXP_CLS struct spp::asts::ObjectInitializerArgumentKeywordAst final : ObjectInitializerArgumentAst {
  SPP_AST_KEY_FUNCTIONS(ObjectInitializerArgumentKeywordAst);

  /// The "=" token, separating the argument name from the
  /// expression passed as the argument's value.
  Unique<TokenAst> TokAssign;

  ObjectInitializerArgumentKeywordAst(
    decltype(Name) name,
    decltype(TokAssign) &&tok_assign,
    decltype(Val) &&val);

  ~ObjectInitializerArgumentKeywordAst() override;
};
