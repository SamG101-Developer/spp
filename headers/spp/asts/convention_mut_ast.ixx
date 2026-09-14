module;
#include <spp/macros.hpp>

export module spp.asts.convention_mut_ast;
import spp.asts.ast;
import spp.asts.ast_kind;
import spp.asts.convention_ast;
import spp.utils.types;
import std;

SPP_AST_COMMON_FWD_DECL(ConventionMutAst);
use(spp::asts, struct TokenAst);

/// A convention for mutable borrows. If the borrow is for an
/// argument, its symbol must be mutably defined.
SPP_EXP_CLS struct spp::asts::ConventionMutAst final : ConventionAst {
  SPP_AST_KEY_FUNCTIONS(ConventionMutAst);

  /// The "&" borrow marker, showing a borrow of some convention
  /// is being made.
  Unique<TokenAst> TokBorrow;

  /// The "mut" keyword, showing the borrow is mutable, so the
  /// value can be modified.
  Unique<TokenAst> TokMut;

  ConventionMutAst(
    decltype(TokBorrow) &&tok_borrow,
    decltype(TokMut) &&tok_mut);

  ~ConventionMutAst() override;
};
