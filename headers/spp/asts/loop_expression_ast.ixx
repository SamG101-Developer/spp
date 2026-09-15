module;
#include <spp/macros.hpp>

export module spp.asts.loop_expression_ast;
import spp.asts.primary_expression_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(LoopExpressionAst);
use(spp::asts, struct LoopElseStatementAst);
use(spp::asts, struct InnerScopeExpressionAst);
use(spp::asts, struct TokenAst);
use(spp::asts, struct TypeAst);
use(spp::analyse::scopes, class Scope);

SPP_EXP_CLS struct spp::asts::LoopExpressionAst : PrimaryExpressionAst {
protected:
  std::optional<Tup<ExpressionAst*, Shared<TypeAst>, Scope*>> m_loop_exit_type_info;

public:
  /// The "loop" token starting the loop expression.
  Unique<TokenAst> TokLoop;

  /// The body of the loop, executed for each iteration.
  Unique<InnerScopeExpressionAst> Body;

  /// The optional "else" block of the loop, executed if the
  /// original condition is immediately false, or the iterable
  /// is already exhausted (no loops take place).
  Unique<LoopElseStatementAst> ElseBlock;

  LoopExpressionAst(
    decltype(TokLoop) &&tok_loop,
    decltype(Body) &&body,
    decltype(ElseBlock) &&else_block);

  ~LoopExpressionAst() override;

  auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;
};
