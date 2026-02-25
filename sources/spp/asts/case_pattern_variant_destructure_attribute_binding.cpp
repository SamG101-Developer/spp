module spp.asts.case_pattern_variant_destructure_attribute_binding_ast;
import spp.asts.identifier_ast;


spp::asts::CasePatternVariantDestructureAttributeBindingAst::CasePatternVariantDestructureAttributeBindingAst(
    decltype(name) &&name,
    decltype(tok_assign) &&tok_assign,
    decltype(val) &&val) :
    name(std::move(name)),
    tok_assign(std::move(tok_assign)),
    val(std::move(val)) {
}


spp::asts::CasePatternVariantDestructureAttributeBindingAst::~CasePatternVariantDestructureAttributeBindingAst() = default;


auto spp::asts::CasePatternVariantDestructureAttributeBindingAst::to_rust() const
    -> std::string {
    return std::format("{} = {}", name->to_rust(), val->to_rust());
}
