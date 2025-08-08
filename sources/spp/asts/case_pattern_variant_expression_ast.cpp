#include <spp/asts/case_pattern_variant_expression_ast.hpp>
#include <spp/asts/expression_ast.hpp>


spp::asts::CasePatternVariantExpressionAst::CasePatternVariantExpressionAst(
    decltype(expr) &&expr):
    expr(std::move(expr)) {
}


spp::asts::CasePatternVariantExpressionAst::~CasePatternVariantExpressionAst() = default;


auto spp::asts::CasePatternVariantExpressionAst::pos_start() const -> std::size_t {
    return expr->pos_start();
}


auto spp::asts::CasePatternVariantExpressionAst::pos_end() const -> std::size_t {
    return expr->pos_end();
}


spp::asts::CasePatternVariantExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(expr);
    SPP_STRING_END;
}


auto spp::asts::CasePatternVariantExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(expr);
    SPP_PRINT_END;
}
