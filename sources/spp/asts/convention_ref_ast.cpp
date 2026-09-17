module;
#include <spp/macros.hpp>

module spp.asts.convention_ref_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
ConventionRefAst::ConventionRefAst(
  decltype(TokBorrow) &&tok_borrow) :
  ConventionAst(ConventionTag::REF),
  TokBorrow(std::move(tok_borrow)) {
  using lex::SppTokenType;
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(
    this->TokBorrow, SppTokenType::TK_BORROW, "&");
}

ConventionRefAst::~ConventionRefAst() = default;

auto ConventionRefAst::PosStart() const -> std::size_t {
  // Use the "&" token.
  return TokBorrow->PosStart();
}

auto ConventionRefAst::PosEnd() const -> std::size_t {
  // Use the "&" token.
  return TokBorrow->PosEnd();
}

auto ConventionRefAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<ConventionRefAst>(
    AstClone(TokBorrow));
}

auto ConventionRefAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokBorrow);
  SPP_STRING_END;
}

SPP_MOD_END
