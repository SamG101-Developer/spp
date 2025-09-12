#include <spp/asts/type_unary_expression_operator_ast.hpp>
#include <spp/asts/type_unary_expression_operator_borrow_ast.hpp>
#include <spp/asts/type_unary_expression_operator_namespace_ast.hpp>


auto spp::asts::TypeUnaryExpressionOperatorAst::operator<=>(
    TypeUnaryExpressionOperatorAst const &that) const
    -> std::strong_ordering {
    return equals(that);
}


auto spp::asts::TypeUnaryExpressionOperatorAst::operator==(
    TypeUnaryExpressionOperatorAst const &that) const
    -> bool {
    return equals(that) == std::strong_ordering::equal;
}


auto spp::asts::TypeUnaryExpressionOperatorAst::equals_op_borrow(
    TypeUnaryExpressionOperatorBorrowAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}


auto spp::asts::TypeUnaryExpressionOperatorAst::equals_op_namespace(
    TypeUnaryExpressionOperatorNamespaceAst const &) const
    -> std::strong_ordering {
    return std::strong_ordering::less;
}
