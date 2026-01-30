module spp.asts.case_pattern_variant_ast;
import spp.asts.local_variable_ast;
import spp.asts.meta.compiler_meta_data;

auto spp::asts::CasePatternVariantAst::convert_to_variable(
    meta::CompilerMetaData *)
    -> std::unique_ptr<LocalVariableAst> {
    // Default implementation for case pattern variants that do not create variables.
    return nullptr;
}


auto spp::asts::CasePatternVariantAst::stage_9_comptime_resolution(
    ScopeManager *,
    CompilerMetaData *)
    -> void {
    // No behaviour but c++ module issues require this be defined here.
}
