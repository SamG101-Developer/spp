#include <spp/asts/case_pattern_variant_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>


auto spp::asts::CasePatternVariantAst::convert_to_variable(
    mixins::CompilerMetaData *)
    -> std::unique_ptr<LocalVariableAst> {
    return nullptr;
}
