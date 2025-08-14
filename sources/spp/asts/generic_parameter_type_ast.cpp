#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/scopes/symbols.hpp>
#include <spp/asts/generic_parameter_type_ast.hpp>
#include <spp/asts/generic_parameter_type_inline_constraints_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::GenericParameterTypeAst::GenericParameterTypeAst(
    decltype(name) &&name,
    decltype(constraints) &&constraints) :
    name(std::move(name)),
    constraints(std::move(constraints)) {
}


auto spp::asts::GenericParameterTypeAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *) -> void {
    // Create e type symbol for the generic parameter.
    auto sym = std::make_unique<analyse::scopes::TypeSymbol>(name->type_parts().back(), nullptr, nullptr, sm->current_scope, true);
    sm->current_scope->add_symbol(std::move(sym));
}


auto spp::asts::GenericParameterTypeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Analyse the name.
    name->stage_7_analyse_semantics(sm, meta);
}
