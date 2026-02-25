module spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.type_ast;


spp::asts::CasePatternVariantDestructureObjectAst::CasePatternVariantDestructureObjectAst(
    decltype(type) type,
    decltype(elems) &&elems) :
    type(std::move(type)),
    elems(std::move(elems)) {
}


spp::asts::CasePatternVariantDestructureObjectAst::~CasePatternVariantDestructureObjectAst() = default;


auto spp::asts::CasePatternVariantDestructureObjectAst::to_rust() const
    -> std::string {
    auto rust_elems = elems | std::views::transform([](const auto &elem) { return elem->to_rust(); });
    return std::format("{} {{ {} }}", type->to_rust(), std::views::join(rust_elems, ", "));
}
