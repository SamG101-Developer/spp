module;
#include <spp/macros.hpp>

module spp.asts.type_postfix_expression_operator_nested_type_ast;
import spp.asts.ast;
import spp.asts.identifier_ast;
import spp.asts.token_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_postfix_expression_operator_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;

SPP_MOD_BEGIN
TypePostfixExpressionOperatorNestedTypeAst::TypePostfixExpressionOperatorNestedTypeAst(
  decltype(TokSep) &&tok_sep,
  decltype(Name) name) :
  TokSep(std::move(tok_sep)),
  Name(std::move(name)) {
  SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->TokSep, lex::SppTokenType::TK_DOUBLE_COLON, "::");
}

TypePostfixExpressionOperatorNestedTypeAst::~TypePostfixExpressionOperatorNestedTypeAst() = default;

auto TypePostfixExpressionOperatorNestedTypeAst::EqualsNestedType(
  TypePostfixExpressionOperatorNestedTypeAst const &other) const -> Ordering {
  // Equality is based on the internal name.
  return *Name <=> *other.Name;
}

auto TypePostfixExpressionOperatorNestedTypeAst::Equals(
  const TypePostfixExpressionOperatorAst &other) const -> Ordering {
  // Reverse hook (double dispatch).
  return other.EqualsNestedType(*this);
}

auto TypePostfixExpressionOperatorNestedTypeAst::PosStart() const -> std::size_t {
  // Use the "::" token.
  return TokSep->PosStart();
}

auto TypePostfixExpressionOperatorNestedTypeAst::PosEnd() const -> std::size_t {
  // Use the name.
  return Name->PosEnd();
}

auto TypePostfixExpressionOperatorNestedTypeAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<TypePostfixExpressionOperatorNestedTypeAst>(
    AstClone(TokSep),
    AstCloneShared(Name));
}

auto TypePostfixExpressionOperatorNestedTypeAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(TokSep);
  SPP_STRING_APPEND(Name);
  SPP_STRING_END;
}

auto TypePostfixExpressionOperatorNestedTypeAst::NsParts() const -> Vec<IdentifierAst const*> {
  return {};
}

auto TypePostfixExpressionOperatorNestedTypeAst::NsParts() -> Vec<IdentifierAst*> {
  return {};
}

auto TypePostfixExpressionOperatorNestedTypeAst::TypeParts() const -> Vec<TypeIdentifierAst const*> {
  return {Name.get()};
}

auto TypePostfixExpressionOperatorNestedTypeAst::TypeParts() -> Vec<TypeIdentifierAst*> {
  return {Name.get()};
}

auto TypePostfixExpressionOperatorNestedTypeAst::NsPartsInto(
  Vec<IdentifierAst const*>&) const -> void {
}

auto TypePostfixExpressionOperatorNestedTypeAst::TypePartsInto(
  Vec<TypeIdentifierAst const*> &out) const -> void {
  out.EmplaceBack(Name.get());
}

SPP_MOD_END
