module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_skip_multiple_arguments_ast;
import spp.asts.ast_kind;
import spp.asts.local_variable_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(LocalVariableDestructureSkipMultipleArgumentsAst);
use(spp::asts, struct CasePatternVariantDestructureSkipMultipleArgumentsAst);
use(spp::asts, struct LocalVariableSingleIdentifierAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TokenAst);

SPP_EXP_CLS struct spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst final : LocalVariableAst {
  SPP_AST_KEY_FUNCTIONS(LocalVariableDestructureSkipMultipleArgumentsAst);

  /// The ".." token marking a group of skipped arguments.
  /// Bindings are used for array and tuple destructuring,
  /// while object destructuring can only use an unbound
  /// multi skip.
  Unique<TokenAst> TokEllipsis;

  /// The optional binding for the skipped arguments, which
  /// collects them into an inner array or tuple (based on the
  /// outer type being destructured). No binding means these
  /// values are dropped.
  Unique<LocalVariableSingleIdentifierAst> Binding;

  LocalVariableDestructureSkipMultipleArgumentsAst(
    decltype(TokEllipsis) &&tok_ellipsis,
    Unique<LocalVariableAst> &&binding); // cast in ctor

  ~LocalVariableDestructureSkipMultipleArgumentsAst() override;

  SPP_ATTR_NODISCARD auto TakesRest() const -> bool override;

  SPP_ATTR_NODISCARD auto ExtractNames() const -> Vec<Shared<IdentifierAst>> override;

  SPP_ATTR_NODISCARD auto ExtractName() const -> Shared<IdentifierAst> override;
};
