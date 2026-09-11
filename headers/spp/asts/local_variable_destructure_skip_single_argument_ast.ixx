module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_skip_single_argument_ast;
import spp.asts.ast_kind;
import spp.asts.local_variable_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(LocalVariableDestructureSkipSingleArgumentAst) {
  SPP_EXP_CLS struct IdentifierAst;
  SPP_EXP_CLS struct TokenAst;
}

SPP_EXP_CLS struct spp::asts::LocalVariableDestructureSkipSingleArgumentAst final : LocalVariableAst {
  SPP_AST_KEY_FUNCTIONS(LocalVariableDestructureSkipSingleArgumentAst);

  /**
   * The @c _ token that indicates the skip single argument pattern. This is used to indicate the next element
   * sequentially is being skipped, and is often seen in array and tuple destructuring. Invalid in object
   * destructuring as it is purely keyword based, and not positional.
   */
  Unique<TokenAst> TokUnderscore;

  /**
   * Construct the LocalVariableDestructureSkipSingleArgumentAst with the arguments matching the members.
   * @param tok_underscore The @c _ token that indicates the skip single argument pattern.
   */
  explicit LocalVariableDestructureSkipSingleArgumentAst(
    decltype(TokUnderscore) &&tok_underscore);

  ~LocalVariableDestructureSkipSingleArgumentAst() override;

  SPP_ATTR_NODISCARD auto ExtractNames() const -> Vec<Shared<IdentifierAst>> override;

  SPP_ATTR_NODISCARD auto ExtractName() const -> Shared<IdentifierAst> override;
};
