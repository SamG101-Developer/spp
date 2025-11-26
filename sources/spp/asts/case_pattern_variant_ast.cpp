module spp.asts.case_pattern_variant_ast;


spp::asts::CasePatternVariantAst::~CasePatternVariantAst() = default;


auto spp::asts::CasePatternVariantAst::convert_to_variable(
    meta::CompilerMetaData *)
    -> std::unique_ptr<LocalVariableAst> {
    return nullptr;
}
