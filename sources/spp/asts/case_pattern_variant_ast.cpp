#include <spp/asts/case_pattern_variant_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::CasePatternVariantAst::CasePatternVariantAst():
    m_mapped_let(nullptr) {
}


spp::asts::CasePatternVariantAst::~CasePatternVariantAst() = default;
