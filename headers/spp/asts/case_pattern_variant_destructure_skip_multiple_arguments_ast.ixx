module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_skip_multiple_arguments_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.case_pattern_variant_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(CasePatternVariantDestructureSkipMultipleArgumentsAst) {
  SPP_EXP_CLS struct CasePatternVariantSingleIdentifierAst;
  SPP_EXP_CLS struct LocalVariableAst;
  SPP_EXP_CLS struct TokenAst;
}

SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst final : CasePatternVariantAst {
  SPP_AST_KEY_FUNCTIONS(CasePatternVariantDestructureSkipMultipleArgumentsAst);

  /**
   * The @c .. token indicates the skip multiple arguments pattern. This is used to indicate that a group of arguments
   * is being skipped. Bindings are used for array and tuple destructuring, while object destructuring can only use an
   * unbound multi skip.
   */
  Unique<TokenAst> TokEllipsis;

  /**
   * The optional binding for the skip multiple arguments pattern. This is used to bind the skipped arguments to a
   * variable, as an inner array or tuple (based on the outer type being destructured). No binding means that these
   * values are dropped.
   */
  Unique<CasePatternVariantSingleIdentifierAst> Binding;

  /**
   * Construct the CasePatternVariantDestructureSkipMultipleArgumentsAst with the arguments matching the members.
   * @param tok_ellipsis The @c .. token that indicates the skip multiple arguments pattern.
   * @param binding The optional binding for the skip multiple arguments pattern.
   */
  CasePatternVariantDestructureSkipMultipleArgumentsAst(
    decltype(TokEllipsis) &&tok_ellipsis,
    Unique<CasePatternVariantAst> &&binding);

  ~CasePatternVariantDestructureSkipMultipleArgumentsAst() override;

  auto ConvToVar(meta::CompilerMetaData *meta) -> Unique<LocalVariableAst> override;
};
