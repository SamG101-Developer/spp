module spp.asts.case_pattern_variant_destructure_tuple_ast;


spp::asts::CasePatternVariantDestructureTupleAst::CasePatternVariantDestructureTupleAst(
    decltype(elems) &&elems) :
    elems(std::move(elems)) {
}


spp::asts::CasePatternVariantDestructureTupleAst::~CasePatternVariantDestructureTupleAst() = default;


auto spp::asts::CasePatternVariantDestructureTupleAst::to_rust() const
    -> std::string {
    auto rust_elems = elems | std::views::transform([](const auto &elem) { return elem->to_rust(); });
    return std::format("({})", std::views::join(rust_elems, ", "));
}
