module;
#include <spp/macros.hpp>

export module spp.asts.convention_ref_ast;
import spp.asts.ast_kind;
import spp.asts.convention_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(ConventionRefAst);
use(spp::asts, struct TokenAst);

/// A convention for immutable borrows, which can be taken from
/// immutable or mutable values.
SPP_EXP_CLS struct spp::asts::ConventionRefAst final : ConventionAst {
  SPP_AST_KEY_FUNCTIONS(ConventionRefAst);

  /// The "&" borrow marker, showing a borrow of some convention
  /// is being made.
  Unique<TokenAst> TokBorrow;

  explicit ConventionRefAst(
    decltype(TokBorrow) &&tok_borrow);

  ~ConventionRefAst() override;
};
