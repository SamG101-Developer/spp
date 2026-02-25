module spp.asts.case_pattern_variant_destructure_skip_multiple_arguments_ast;
import spp.asts.case_pattern_variant_single_identifier_ast;


spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::CasePatternVariantDestructureSkipMultipleArgumentsAst(
    decltype(tok_ellipsis) &&tok_ellipsis,
    std::unique_ptr<CasePatternVariantAst> &&binding) :
    tok_ellipsis(std::move(tok_ellipsis)),
    binding(std::unique_ptr<CasePatternVariantSingleIdentifierAst>(binding ? binding.release()->to<CasePatternVariantSingleIdentifierAst>() : nullptr)) {
}


spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::~CasePatternVariantDestructureSkipMultipleArgumentsAst() = default;


auto spp::asts::CasePatternVariantDestructureSkipMultipleArgumentsAst::to_rust() const
    -> std::string {
    if (binding == nullptr) {
        return "..";
    }
    return std::format("{} @ ..", binding->to_rust());
}
