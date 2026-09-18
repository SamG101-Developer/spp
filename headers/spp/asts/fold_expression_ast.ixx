module;
#include <spp/macros.hpp>

export module spp.asts.fold_expression_ast;
import spp.asts.ast_kind;
import spp.asts.primary_expression_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(FoldExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, struct TypeRef);

SPP_EXP_CLS struct spp::asts::FoldExpressionAst final : PrimaryExpressionAst {
  SPP_AST_KEY_FUNCTIONS(FoldExpressionAst);

  /// The ".." fold token that indicates a fold operation. Used
  /// in binary and function call contexts.
  Unique<TokenAst> TokEllipsis;

  explicit FoldExpressionAst(
    decltype(TokEllipsis) &&tok_ellipsis);

  ~FoldExpressionAst() override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;

  auto InferTypeRef(ScopeManager *sm, CompilerMetaData *meta) -> TypeRef override;
};
