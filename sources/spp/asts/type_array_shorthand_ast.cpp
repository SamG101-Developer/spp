module;
#include <spp/macros.hpp>

module spp.asts.type_array_shorthand_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
TypeArrayShorthandAst::TypeArrayShorthandAst(
  decltype(TokL) &&tok_l,
  decltype(ElemType) &&element_type,
  decltype(TokSemiColon) &&tok_semicolon,
  decltype(Size) &&size,
  decltype(TokR) &&tok_r) :
  TokL(std::move(tok_l)),
  ElemType(std::move(element_type)),
  TokSemiColon(std::move(tok_semicolon)),
  Size(std::move(size)),
  TokR(std::move(tok_r)) {
}

TypeArrayShorthandAst::~TypeArrayShorthandAst() = default;

auto TypeArrayShorthandAst::PosStart() const -> std::size_t {
  // Use the "[" token.
  return TokL->PosStart();
}

auto TypeArrayShorthandAst::PosEnd() const -> std::size_t {
  // Use the "]" token.
  return TokR->PosEnd();
}

auto TypeArrayShorthandAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<TypeArrayShorthandAst>(
    AstClone(TokL),
    AstCloneShared(ElemType),
    AstClone(TokSemiColon),
    AstClone(Size),
    AstClone(TokR));
}

auto TypeArrayShorthandAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND_RAW("[");
  SPP_STRING_APPEND(ElemType);
  SPP_STRING_APPEND_RAW(";");
  SPP_STRING_APPEND(Size);
  SPP_STRING_APPEND_RAW("]");
  SPP_STRING_END;
}

auto TypeArrayShorthandAst::Convert() -> Unique<TypeAst> {
  // Convert to the compiler-known array type.
  using generate::common_types::ArrayType;
  const auto type = ArrayType(PosStart(), std::move(ElemType), std::move(Size));
  return AstClone(type);
}

SPP_MOD_END
