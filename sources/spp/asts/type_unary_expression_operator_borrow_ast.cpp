#include <spp/asts/type_unary_expression_operator_borrow_ast.hpp>
#include <spp/asts/convention_ast.hpp>


spp::asts::TypeUnaryExpressionOperatorBorrowAst::TypeUnaryExpressionOperatorBorrowAst(
    decltype(conv) &&conv):
    conv(std::move(conv)) {
}


auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::pos_start() const -> std::size_t {
    return conv->pos_start();
}


auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::pos_end() const -> std::size_t {
    return conv->pos_end();
}


spp::asts::TypeUnaryExpressionOperatorBorrowAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(conv);
    SPP_STRING_END;
}


auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(conv);
    SPP_PRINT_END;
}


auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::ns_parts() const -> std::vector<IdentifierAst const *> {
    return {};
}


auto spp::asts::TypeUnaryExpressionOperatorBorrowAst::type_parts() const -> std::vector<TypeIdentifierAst const *> {
    return {};
}
