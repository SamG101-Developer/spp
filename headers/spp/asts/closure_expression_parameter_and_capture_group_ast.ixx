module;
#include <spp/macros.hpp>

export module spp.asts.closure_expression_parameter_and_capture_group_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(ClosureExpressionParameterAndCaptureGroupAst);
use(spp::asts, struct FunctionParameterAst);
use(spp::asts, struct FunctionParameterGroupAst);
use(spp::asts, struct ClosureExpressionCaptureGroupAst);
use(spp::asts, struct TokenAst);

namespace spp::asts {
  SPP_EXP_CLS
  using ClosureExpressionParameterGroupAst = FunctionParameterGroupAst;
}

SPP_EXP_CLS struct spp::asts::ClosureExpressionParameterAndCaptureGroupAst final : Ast {
  SPP_AST_KEY_FUNCTIONS(ClosureExpressionParameterAndCaptureGroupAst);

  /// The "|" token that starts the parameter and capture group.
  Unique<TokenAst> TokL;

  /// The parameters passed to the closure when it is called.
  Unique<ClosureExpressionParameterGroupAst> ParamGroup;

  /// The variables captured from the outer scope, which can be
  /// used within the closure's body.
  Unique<ClosureExpressionCaptureGroupAst> CaptureGroup;

  /// The "|" token that ends the parameter and capture group.
  Unique<TokenAst> TokR;

  ClosureExpressionParameterAndCaptureGroupAst(
    decltype(TokL) &&tok_l,
    decltype(ParamGroup) &&param_group,
    decltype(CaptureGroup) &&capture_group,
    decltype(TokR) &&tok_r);

  ~ClosureExpressionParameterAndCaptureGroupAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;
};
