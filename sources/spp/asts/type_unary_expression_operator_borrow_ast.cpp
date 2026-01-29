module;
#include <spp/macros.hpp>

module spp.asts.type_unary_expression_operator_borrow_ast;
import spp.asts.convention_ast;
import spp.asts.utils.ast_utils;


spp::asts::TypeUnaryExpressionOperatorBorrowAst::TypeUnaryExpressionOperatorBorrowAst(
    decltype(conv) &&conv) :
    conv(std::move(conv)) {
}


auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::equals(
    TypeUnaryExpressionOperatorAst const &other) const
    -> std::strong_ordering {
    // Double dispatch to the appropriate equals method.
    return other.equals_op_borrow(*this);
}


auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::equals_op_borrow(
    TypeUnaryExpressionOperatorBorrowAst const &other) const
    -> std::strong_ordering {
    // Check if the conventions are the same.
    if (*conv == other.conv.get()) {
        return std::strong_ordering::equal;
    }
    return std::strong_ordering::less;
}


auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::pos_start() const
    -> std::size_t {
    return conv->pos_start();
}


auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::pos_end() const
    -> std::size_t {
    return conv->pos_end();
}


auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<TypeUnaryExpressionOperatorBorrowAst>(
        ast_clone(conv));
}


spp::asts::TypeUnaryExpressionOperatorBorrowAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(conv);
    SPP_STRING_END;
}


auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::ns_parts() const
    -> std::vector<std::shared_ptr<const IdentifierAst>> {
    return {};
}


auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::ns_parts()
    -> std::vector<std::shared_ptr<IdentifierAst>> {
    return {};
}


auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::type_parts() const
    -> std::vector<std::shared_ptr<const TypeIdentifierAst>> {
    return {};
}


auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::type_parts()
    -> std::vector<std::shared_ptr<TypeIdentifierAst>> {
    return {};
}
