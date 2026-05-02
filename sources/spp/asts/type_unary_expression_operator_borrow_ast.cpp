module;
#include <spp/macros.hpp>

module spp.asts.type_unary_expression_operator_borrow_ast;
import spp.asts.convention_ast;
import spp.asts.identifier_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::TypeUnaryExpressionOperatorBorrowAst::TypeUnaryExpressionOperatorBorrowAst(
    decltype(Conv) &&conv) :
    Conv(std::move(conv)) {
}

spp::asts::TypeUnaryExpressionOperatorBorrowAst::~TypeUnaryExpressionOperatorBorrowAst() = default;

auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::EqualsOpBorrow(
    TypeUnaryExpressionOperatorBorrowAst const &other) const
    -> Ordering {
    // Equality is based on the convention.
    return *Conv == other.Conv.get() ? Ordering::equal : Ordering::less;
}

auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::Equals(
    TypeUnaryExpressionOperatorAst const &other) const
    -> Ordering {
    // Double dispatch to the appropriate equals method.
    return other.EqualsOpBorrow(*this);
}

auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::PosStart() const
    -> std::size_t {
    // Use the convention.
    return Conv->PosStart();
}

auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::PosEnd() const
    -> std::size_t {
    // Use the convention.
    return Conv->PosEnd();
}

auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<TypeUnaryExpressionOperatorBorrowAst>(
        AstClone(Conv));
}

auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(Conv);
    SPP_STRING_END;
}

auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::NsParts() const
    -> Vec<Shared<const IdentifierAst>> {
    return {};
}

auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::NsParts()
    -> Vec<Shared<IdentifierAst>> {
    return {};
}

auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::TypeParts() const
    -> Vec<Shared<const TypeIdentifierAst>> {
    return {};
}

auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::TypeParts()
    -> Vec<Shared<TypeIdentifierAst>> {
    return {};
}

SPP_MOD_END
