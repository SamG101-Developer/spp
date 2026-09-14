module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_skip_single_argument_ast;
import spp.asts.ast_kind;
import spp.asts.case_pattern_variant_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(CasePatternVariantDestructureSkipSingleArgumentAst);
use(spp::asts, struct LocalVariableAst);
use(spp::asts, struct TokenAst);

SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst final : CasePatternVariantAst {
  SPP_AST_KEY_FUNCTIONS(CasePatternVariantDestructureSkipSingleArgumentAst);

  /// The "_" token, skipping the next element sequentially,
  /// as seen in array and tuple destructuring. Invalid in
  /// object destructuring, which is purely keyword based, not
  /// positional.
  Unique<TokenAst> TokUnderscore;

  explicit CasePatternVariantDestructureSkipSingleArgumentAst(
    decltype(TokUnderscore) &&tok_underscore);

  ~CasePatternVariantDestructureSkipSingleArgumentAst() override;

  auto ConvToVar(CompilerMetaData *meta) -> Unique<LocalVariableAst> override;
};
