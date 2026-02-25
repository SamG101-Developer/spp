module spp.asts.case_pattern_variant_destructure_array_ast;


spp::asts::CasePatternVariantDestructureArrayAst::CasePatternVariantDestructureArrayAst(
    decltype(elems) &&elems) :
    elems(std::move(elems)) {
}


spp::asts::CasePatternVariantDestructureArrayAst::~CasePatternVariantDestructureArrayAst() = default;


auto spp::asts::CasePatternVariantDestructureArrayAst::to_rust() const
    -> std::string {
    auto rust_elems = elems | std::views::transform([](const auto &elem) { return elem->to_rust(); });
    return std::format("[{}]", std::views::join(rust_elems, ", "));
}
