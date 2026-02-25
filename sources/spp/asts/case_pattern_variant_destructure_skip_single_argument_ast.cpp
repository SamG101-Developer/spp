module spp.asts.case_pattern_variant_destructure_skip_single_argument_ast;


spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst::CasePatternVariantDestructureSkipSingleArgumentAst(
    decltype(tok_underscore) &&tok_underscore) :
    tok_underscore(std::move(tok_underscore)) {
}


spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst::~CasePatternVariantDestructureSkipSingleArgumentAst() = default;


auto spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst::to_rust() const
    -> std::string {
    return "_";
}
