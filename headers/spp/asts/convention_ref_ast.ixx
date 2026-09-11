module;
#include <spp/macros.hpp>

export module spp.asts.convention_ref_ast;
import spp.asts.ast_kind;
import spp.asts.convention_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(ConventionRefAst) {
  SPP_EXP_CLS struct TokenAst;
}

/**
 * The ConventionRefAst represents a convention for immutable borrows. Immutable borrows can be taken from immutable or
 * mutable values.
 */
SPP_EXP_CLS struct spp::asts::ConventionRefAst final : ConventionAst {
  SPP_AST_KEY_FUNCTIONS(ConventionRefAst);

  /**
   * The token that represents the @c & borrow marker. This is used to indicate that a borrow of some convention is
   * being made.
   */
  Unique<TokenAst> TokBorrow;

  /**
   * Construct the ConventionRefAst with the arguments matching the members.
   * @param tok_borrow The token that represents the @c & borrow marker.
   */
  explicit ConventionRefAst(
    decltype(TokBorrow) &&tok_borrow);

  ~ConventionRefAst() override;
};
