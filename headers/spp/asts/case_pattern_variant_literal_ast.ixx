module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_literal_ast;
import spp.asts.ast_kind;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(CasePatternVariantLiteralAst) {
  SPP_EXP_CLS struct LiteralAst;
  SPP_EXP_CLS struct LocalVariableAst;
}

SPP_EXP_CLS struct spp::asts::CasePatternVariantLiteralAst final : CasePatternVariantAst {
  SPP_AST_KEY_FUNCTIONS(CasePatternVariantLiteralAst);

  /**
   * The literal value of the case pattern variant. This can be a string, integer, float, boolean, but not a tuple or
   * array; special destructure syntax exists for those literals.
   */
  Unique<LiteralAst> Literal;

  /**
   * Construct the CasePatternVariantLiteralAst with the arguments matching the members.
   * @param literal The literal value of the case pattern variant.
   */
  explicit CasePatternVariantLiteralAst(
    decltype(Literal) &&literal);

  ~CasePatternVariantLiteralAst() override;

  auto Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void override;

  auto Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LlvmCtx *ctx)
    -> llvm::Value* override;

  auto ConvToVar(meta::CompilerMetaData *meta) -> Unique<LocalVariableAst> override;
};
