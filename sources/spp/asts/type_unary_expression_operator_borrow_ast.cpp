module;
#include <spp/macros.hpp>

module spp.asts.type_unary_expression_operator_borrow_ast;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
TypeUnaryExpressionOperatorBorrowAst::TypeUnaryExpressionOperatorBorrowAst(
  decltype(Conv) &&conv) :
  Conv(std::move(conv)) {
}

TypeUnaryExpressionOperatorBorrowAst::~TypeUnaryExpressionOperatorBorrowAst() = default;

auto TypeUnaryExpressionOperatorBorrowAst::EqualsOpBorrow(
  TypeUnaryExpressionOperatorBorrowAst const &other) const -> Ordering {
  // Equality is based on the convention.
  return *Conv == other.Conv.get() ? Ordering::equal : Ordering::less;
}

auto TypeUnaryExpressionOperatorBorrowAst::Equals(
  TypeUnaryExpressionOperatorAst const &other) const -> Ordering {
  // Double dispatch to the appropriate equals method.
  return other.EqualsOpBorrow(*this);
}

auto TypeUnaryExpressionOperatorBorrowAst::PosStart() const -> std::size_t {
  // Use the convention.
  return Conv->PosStart();
}

auto TypeUnaryExpressionOperatorBorrowAst::PosEnd() const -> std::size_t {
  // Use the convention.
  return Conv->PosEnd();
}

auto TypeUnaryExpressionOperatorBorrowAst::Clone() const -> Unique<Ast> {
  // Clone all the members of the ast.
  return MakeUnique<TypeUnaryExpressionOperatorBorrowAst>(
    AstClone(Conv));
}

auto TypeUnaryExpressionOperatorBorrowAst::ToString() const -> Str {
  SPP_STRING_START;
  SPP_STRING_APPEND(Conv);
  SPP_STRING_END;
}

auto TypeUnaryExpressionOperatorBorrowAst::NsParts() const -> Vec<IdentifierAst const*> {
  return {};
}

auto TypeUnaryExpressionOperatorBorrowAst::NsParts() -> Vec<IdentifierAst*> {
  return {};
}

auto TypeUnaryExpressionOperatorBorrowAst::TypeParts() const -> Vec<TypeIdentifierAst const*> {
  return {};
}

auto TypeUnaryExpressionOperatorBorrowAst::TypeParts() -> Vec<TypeIdentifierAst*> {
  return {};
}

SPP_MOD_END
