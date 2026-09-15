module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_skip_multiple_arguments_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.case_pattern_variant_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(CasePatternVariantDestructureSkipMultipleArgumentsAst);
use(spp::asts, struct CasePatternVariantSingleIdentifierAst);
use(spp::asts, struct LocalVariableAst);
use(spp::asts, struct TokenAst);

SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst final : CasePatternVariantAst {
  SPP_AST_KEY_FUNCTIONS(CasePatternVariantDestructureSkipMultipleArgumentsAst);

  /// The ".." token, skipping a group of arguments. Bindings
  /// are used for array and tuple destructuring, while object
  /// destructuring can only use an unbound multi skip.
  Unique<TokenAst> TokEllipsis;

  /// The optional binding of the skipped arguments to a
  /// variable, as an inner array or tuple (based on the outer
  /// type being destructured). No binding means the values
  /// are dropped.
  Unique<CasePatternVariantSingleIdentifierAst> Binding;

  CasePatternVariantDestructureSkipMultipleArgumentsAst(
    decltype(TokEllipsis) &&tok_ellipsis,
    Unique<CasePatternVariantAst> &&binding);

  ~CasePatternVariantDestructureSkipMultipleArgumentsAst() override;

  auto ConvToVar(CompilerMetaData *meta) -> Unique<LocalVariableAst> override;
};
