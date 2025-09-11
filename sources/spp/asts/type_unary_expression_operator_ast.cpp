#include <spp/asts/type_unary_expression_operator_ast.hpp>
#include <spp/asts/type_unary_expression_operator_borrow_ast.hpp>
#include <spp/asts/type_unary_expression_operator_namespace_ast.hpp>


auto spp::asts::TypeUnaryExpressionOperatorAst::equals_op_borrow(
    TypeUnaryExpressionOperatorBorrowAst const &) const
    -> std::weak_ordering {
    return std::weak_ordering::less;
}


auto spp::asts::TypeUnaryExpressionOperatorAst::equals_op_namespace(
    TypeUnaryExpressionOperatorNamespaceAst const &) const
    -> std::weak_ordering {
    return std::weak_ordering::less;
}
