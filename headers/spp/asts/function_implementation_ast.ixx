module;
#include <spp/macros.hpp>

export module spp.asts.function_implementation_ast;
import spp.asts.inner_scope_expression_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(FunctionImplementationAst);
use(spp::asts, struct Ast);

/// The implementation of a function: the body holding the
/// statements that make up the function. Semantically
/// equivalent to a basic InnerScopeAst.
SPP_EXP_CLS struct spp::asts::FunctionImplementationAst : InnerScopeExpressionAst {
  static auto NewEmpty() -> Unique<FunctionImplementationAst>;

  using InnerScopeExpressionAst::InnerScopeExpressionAst;

  SPP_ATTR_NODISCARD auto Clone() const -> Unique<Ast> override;

  ~FunctionImplementationAst() override;

  SPP_ATTR_NODISCARD auto DiscardsFinalMember() const -> bool override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
