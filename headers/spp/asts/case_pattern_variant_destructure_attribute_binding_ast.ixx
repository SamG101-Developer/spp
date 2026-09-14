module;
#include <spp/macros.hpp>

export module spp.asts.case_pattern_variant_destructure_attribute_binding_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.case_pattern_variant_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(CasePatternVariantDestructureAttributeBindingAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct LocalVariableAst);
use(spp::asts, struct TokenAst);

SPP_EXP_CLS struct spp::asts::CasePatternVariantDestructureAttributeBindingAst final : CasePatternVariantAst {
  SPP_AST_KEY_FUNCTIONS(CasePatternVariantDestructureAttributeBindingAst);

  /// The name of the attribute of the object being
  /// destructured.
  Shared<IdentifierAst> Name;

  /// The "=" token separating the attribute name from the
  /// value in the destructure binding.
  Unique<TokenAst> TokAssign;

  /// The value of the attribute. This can be a further
  /// destructure or a literal.
  Unique<CasePatternVariantAst> Val;

  CasePatternVariantDestructureAttributeBindingAst(
    decltype(Name) &&name,
    decltype(TokAssign) &&tok_assign,
    decltype(Val) &&val);

  ~CasePatternVariantDestructureAttributeBindingAst() override;

  SPP_ATTR_NODISCARD auto BindsByMove() const -> bool override;

  auto ConvToVar(CompilerMetaData *meta) -> Unique<LocalVariableAst> override;
};
