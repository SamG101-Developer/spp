module;
#include <spp/macros.hpp>

export module spp.asts.closure_expression_capture_group_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(ClosureExpressionCaptureGroupAst);
use(spp::asts, struct ClosureExpressionCaptureAst);
use(spp::asts, struct TokenAst);

SPP_EXP_CLS struct spp::asts::ClosureExpressionCaptureGroupAst final : Ast {
  SPP_AST_KEY_FUNCTIONS(ClosureExpressionCaptureGroupAst);

  /// The "caps" token that starts the capture group, showing the
  /// closure has moved on from parameter definitions and is now
  /// capturing variables from the outer scope.
  Unique<TokenAst> TokCaps;

  /// The variables captured from the outer scope, which can be
  /// used within the closure's body.
  Vec<Unique<ClosureExpressionCaptureAst>> Captures;

  static auto NewEmpty() -> Unique<ClosureExpressionCaptureGroupAst>;

  explicit ClosureExpressionCaptureGroupAst(
    decltype(TokCaps) &&tok_caps,
    decltype(Captures) &&captures);

  ~ClosureExpressionCaptureGroupAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;
};
