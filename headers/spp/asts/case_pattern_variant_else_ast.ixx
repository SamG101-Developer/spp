module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_else_ast;
import spp.asts.ast_kind;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(CasePatternVariantElseAst);
use(spp::asts, struct TokenAst);

SPP_EXP_CLS struct spp::asts::CasePatternVariantElseAst final : CasePatternVariantAst {
  SPP_AST_KEY_FUNCTIONS(CasePatternVariantElseAst);

  /// The "else" keyword marking this as an else branch of the
  /// case pattern variant.
  Unique<TokenAst> TokElse;

  explicit CasePatternVariantElseAst(
    decltype(TokElse) &&tok_else);

  ~CasePatternVariantElseAst() override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto MarkForIterLoopExit() -> void;

  SPP_ATTR_NODISCARD auto MarkedForIterLoopExit() const -> bool;

private:
  bool _ForIterLoopExit;
};
