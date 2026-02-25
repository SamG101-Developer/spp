module spp.asts.case_pattern_variant_else_ast;


spp::asts::CasePatternVariantElseAst::CasePatternVariantElseAst(
    decltype(tok_else) &&tok_else) :
    tok_else(std::move(tok_else)) {
}


spp::asts::CasePatternVariantElseAst::~CasePatternVariantElseAst() = default;


auto spp::asts::CasePatternVariantElseAst::to_rust() const
    -> std::string {
    return "else";
}
