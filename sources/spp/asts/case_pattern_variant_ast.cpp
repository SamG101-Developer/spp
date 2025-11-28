module spp.asts.case_pattern_variant_ast;
import spp.asts.local_variable_ast;
import spp.asts.meta.compiler_meta_data;


auto spp::asts::CasePatternVariantAst::convert_to_variable(
    meta::CompilerMetaData *)
    -> std::unique_ptr<LocalVariableAst> {
    return nullptr;
}
