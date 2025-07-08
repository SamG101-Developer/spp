#include <spp/asts/binary_expression_ast.hpp>
#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::BinaryExpressionAst::BinaryExpressionAst(
    decltype(lhs) &&lhs,
    decltype(tok_op) &&tok_op,
    decltype(rhs) &&rhs):
    lhs(std::move(lhs)),
    tok_op(std::move(tok_op)),
    rhs(std::move(rhs)),
    m_mapped_func(nullptr) {
}


auto spp::asts::BinaryExpressionAst::pos_end() -> std::size_t {
    return rhs->pos_end();
}


spp::asts::BinaryExpressionAst::operator icu::UnicodeString() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(lhs);
    SPP_STRING_APPEND(tok_op);
    SPP_STRING_APPEND(rhs);
    SPP_STRING_END;
}


auto spp::asts::BinaryExpressionAst::print(meta::AstPrinter &printer) const -> icu::UnicodeString {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(lhs);
    SPP_PRINT_APPEND(tok_op);
    SPP_PRINT_APPEND(rhs);
    SPP_PRINT_END;
}
