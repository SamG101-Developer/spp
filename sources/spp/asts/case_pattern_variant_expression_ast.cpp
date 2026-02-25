module spp.asts.case_pattern_variant_expression_ast;
import spp.asts.expression_ast;


spp::asts::CasePatternVariantExpressionAst::CasePatternVariantExpressionAst(
    decltype(expr) &&expr) :
    expr(std::move(expr)) {
}


spp::asts::CasePatternVariantExpressionAst::~CasePatternVariantExpressionAst() = default;


auto spp::asts::CasePatternVariantExpressionAst::to_rust() const
    -> std::string {
    return expr->to_rust();
}
