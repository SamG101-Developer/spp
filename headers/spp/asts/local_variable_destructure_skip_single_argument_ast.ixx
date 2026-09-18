module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_skip_single_argument_ast;
import spp.asts.ast_kind;
import spp.asts.local_variable_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(LocalVariableDestructureSkipSingleArgumentAst);
use(spp::asts, struct IdentifierAst);
use(spp::asts, struct TokenAst);

SPP_EXP_CLS struct spp::asts::LocalVariableDestructureSkipSingleArgumentAst final : LocalVariableAst {
  SPP_AST_KEY_FUNCTIONS(LocalVariableDestructureSkipSingleArgumentAst);

  /// The "_" token indicating the next element sequentially is
  /// being skipped, often seen in array and tuple
  /// destructuring. Invalid in object destructuring, as that
  /// is purely keyword based, not positional.
  Unique<TokenAst> TokUnderscore;

  explicit LocalVariableDestructureSkipSingleArgumentAst(
    decltype(TokUnderscore) &&tok_underscore);

  ~LocalVariableDestructureSkipSingleArgumentAst() override;

  SPP_ATTR_NODISCARD auto ExtractNames() const -> Vec<Shared<IdentifierAst>> override;

  SPP_ATTR_NODISCARD auto ExtractName() const -> Shared<IdentifierAst> override;
};
