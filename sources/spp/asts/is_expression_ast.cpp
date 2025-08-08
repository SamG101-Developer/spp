#include <algorithm>

#include <spp/asts/case_pattern_variant_ast.hpp>
#include <spp/asts/is_expression_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::IsExpressionAst::IsExpressionAst(
    decltype(lhs) &&lhs,
    decltype(tok_op) &&tok_op,
    decltype(rhs) &&rhs):
    lhs(std::move(lhs)),
    tok_op(std::move(tok_op)),
    rhs(std::move(rhs)) {
}


spp::asts::IsExpressionAst::~IsExpressionAst() = default;


auto spp::asts::IsExpressionAst::pos_start() const -> std::size_t {
    return lhs->pos_start();
}


auto spp::asts::IsExpressionAst::pos_end() const -> std::size_t {
    return rhs->pos_end();
}


spp::asts::IsExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(lhs);
    SPP_STRING_APPEND(tok_op);
    SPP_STRING_APPEND(rhs);
    SPP_STRING_END;
}


auto spp::asts::IsExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(lhs);
    SPP_PRINT_APPEND(tok_op);
    SPP_PRINT_APPEND(rhs);
    SPP_PRINT_END;
}
