module spp.asts.case_pattern_variant_literal_ast;
import spp.asts.literal_ast;


spp::asts::CasePatternVariantLiteralAst::CasePatternVariantLiteralAst(
    decltype(literal) &&literal) :
    CasePatternVariantAst(),
    literal(std::move(literal)) {
}


spp::asts::CasePatternVariantLiteralAst::~CasePatternVariantLiteralAst() = default;


auto spp::asts::CasePatternVariantLiteralAst::to_rust() const
    -> std::string {
    return literal->to_rust();
}
