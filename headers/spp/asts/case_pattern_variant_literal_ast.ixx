module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_literal_ast;
import spp.asts.ast_kind;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(CasePatternVariantLiteralAst);
use(spp::asts, struct LiteralAst);
use(spp::asts, struct LocalVariableAst);

SPP_EXP_CLS struct spp::asts::CasePatternVariantLiteralAst final : CasePatternVariantAst {
  SPP_AST_KEY_FUNCTIONS(CasePatternVariantLiteralAst);

  /// The literal value of the pattern: a string, integer,
  /// float or boolean, but not a tuple or array; special
  /// destructure syntax exists for those literals.
  Unique<LiteralAst> Literal;

  explicit CasePatternVariantLiteralAst(
    decltype(Literal) &&literal);

  ~CasePatternVariantLiteralAst() override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto ConvToVar(CompilerMetaData *meta) -> Unique<LocalVariableAst> override;
};
