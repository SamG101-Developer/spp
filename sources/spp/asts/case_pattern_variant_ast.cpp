module spp.asts.case_pattern_variant_ast;


auto spp::asts::CasePatternVariantAst::convert_to_variable(
    CompilerMetaData *)
    -> std::unique_ptr<LocalVariableAst> {
    return nullptr;
}
