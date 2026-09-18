module;
#include <spp/macros.hpp>

module spp.asts.type_tuple_shorthand_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;
import genex;

SPP_MOD_BEGIN
TypeTupleShorthandAst::TypeTupleShorthandAst(
  decltype(TokL) &&tok_l,
  decltype(ElemTypes) &&element_types,
  decltype(TokR) &&tok_r) :
  TokL(std::move(tok_l)),
  ElemTypes(std::move(element_types)),
  TokR(std::move(tok_r)) {
}

TypeTupleShorthandAst::~TypeTupleShorthandAst() = default;

auto TypeTupleShorthandAst::PosStart() const -> std::size_t {
  // Use the "(" token.
  return TokL->PosStart();
}

auto TypeTupleShorthandAst::PosEnd() const -> std::size_t {
  // Use the ")" token.
  return TokR->PosEnd();
}

auto TypeTupleShorthandAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<TypeTupleShorthandAst>(
    AstClone(TokL),
    AstCloneVecShared(ElemTypes),
    AstClone(TokR));
}

auto TypeTupleShorthandAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokL);
  SPP_STRING_EXTEND(ElemTypes, ", ");
  SPP_STRING_APPEND(TokR);
  SPP_STRING_END;
}

auto TypeTupleShorthandAst::Convert() -> Unique<TypeAst> {
  using generate::common_types::TupleType;
  const auto type = TupleType(
    PosStart(), std::move(ElemTypes));
  return AstClone(type);
}

SPP_MOD_END
