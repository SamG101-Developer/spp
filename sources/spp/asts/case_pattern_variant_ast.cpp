module;
#include <spp/macros.hpp>

module spp.asts.case_pattern_variant_ast;
import spp.asts.local_variable_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.meta.compiler_meta_data;

SPP_MOD_BEGIN
spp::asts::CasePatternVariantAst::CasePatternVariantAst() :
    _MappedLet(nullptr) {
}

auto spp::asts::CasePatternVariantAst::Stage9_CompTimeResolve(
    ScopeManager *,
    CompilerMetaData *)
    -> void {
    // No behaviour but c++ module issues require this be defined here.
}

auto spp::asts::CasePatternVariantAst::ConvToVar(
    meta::CompilerMetaData *)
    -> Unique<LocalVariableAst> {
    // Default implementation for case pattern variants that do not create variables.
    return nullptr;
}

SPP_MOD_END
