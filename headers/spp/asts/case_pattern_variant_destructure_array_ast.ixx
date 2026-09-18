module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_array_ast;
import spp.asts.ast_kind;
import spp.asts.case_pattern_variant_ast;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

SPP_AST_COMMON_FWD_DECL(CasePatternVariantDestructureArrayAst);
use(spp::asts, struct LocalVariableAst);
use(spp::asts, struct TokenAst);

SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureArrayAst final : CasePatternVariantAst {
  SPP_AST_KEY_FUNCTIONS(CasePatternVariantDestructureArrayAst);

  /// The "[" token starting the array destructuring pattern.
  Unique<TokenAst> TokL;

  /// The patterns destructured from the array. Each element can
  /// be a single identifier, a nested destructuring pattern, or
  /// a literal.
  Vec<Unique<CasePatternVariantAst>> Elems;

  /// The "]" token ending the array destructuring pattern.
  Unique<TokenAst> TokR;

  CasePatternVariantDestructureArrayAst(decltype(TokL) &&tok_l, decltype(Elems) &&elems, decltype(TokR) &&tok_r);

  ~CasePatternVariantDestructureArrayAst() override;

  SPP_ATTR_NODISCARD auto BindsByMove() const -> bool override;

  auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage8_CheckMemory(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

  auto Stage11_CodeGen(ScopeManager *sm, CompilerMetaData *meta, codegen::LlvmCtx *ctx) -> llvm::Value* override;

  auto ConvToVar(CompilerMetaData *meta) -> Unique<LocalVariableAst> override;
};
