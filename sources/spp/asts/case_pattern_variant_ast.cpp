#include <spp/asts/case_pattern_variant_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>


spp::asts::CasePatternVariantAst::~CasePatternVariantAst() = default;


auto spp::asts::CasePatternVariantAst::convert_to_variable(
    mixins::CompilerMetaData *)
    -> std::unique_ptr<LocalVariableAst> {
    return nullptr;
}
