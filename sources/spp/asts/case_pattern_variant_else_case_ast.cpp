module spp.asts.case_pattern_variant_else_case_ast;
import spp.asts.case_expression_ast;


spp::asts::CasePatternVariantElseCaseAst::CasePatternVariantElseCaseAst(
    decltype(tok_else) &&tok_else,
    decltype(case_expr) &&case_expr) :
    tok_else(std::move(tok_else)),
    case_expr(std::move(case_expr)) {
}


spp::asts::CasePatternVariantElseCaseAst::~CasePatternVariantElseCaseAst() = default;


auto spp::asts::CasePatternVariantElseCaseAst::to_rust() const
    -> std::string {
    return std::format("else {}", case_expr->to_rust());
}
