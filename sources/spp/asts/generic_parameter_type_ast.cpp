module;
#include <spp/macros.hpp>

module spp.asts.generic_parameter_type_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.convention_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.utils.ast_utils;


spp::asts::GenericParameterTypeAst::GenericParameterTypeAst(
    decltype(name) name,
    decltype(constraints) &&constraints,
    const utils::OrderableTag order_tag) :
    GenericParameterAst(std::move(name), order_tag),
    constraints(std::move(constraints)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(constraints);
}


auto spp::asts::GenericParameterTypeAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    // Create the generic scope for the generic parameter.
    auto scope_name = analyse::scopes::ScopeBlockName::from_parts(
        "generic-param-type", {name.get()}, pos_start());
    auto generic_scope = std::make_unique<analyse::scopes::Scope>(
        std::move(scope_name), sm->current_scope);

    // Create the type symbol for the generic parameter.
    const auto sym = std::make_shared<analyse::scopes::TypeSymbol>(
        ast_clone(name->type_parts().back().get()), nullptr,
        generic_scope.get(), sm->current_scope, sm->current_scope, true);

    // Link all scopes and symbol information together.
    generic_scope->ty_sym = sym;
    analyse::scopes::ScopeManager::temp_scopes.emplace_back(std::move(generic_scope));
    sm->current_scope->add_type_symbol(sym);
}


auto spp::asts::GenericParameterTypeAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Qualify the name.
    const auto sym = sm->current_scope->get_type_symbol(name->type_parts().back());
    name->stage_4_qualify_types(sm, nullptr);
    if (constraints != nullptr) {
        constraints->stage_7_analyse_semantics(sm, meta);

        // Load the constraints as symbol scopes.
        auto temp_constraints = decltype(constraints->constraints)();
        for (auto const &constraint : constraints->constraints) {
            const auto constraint_sym = sm->current_scope->get_type_symbol(constraint);
            const auto constraint_scope = constraint_sym->scope;
            sym->scope->direct_sup_scopes.emplace_back(constraint_scope);
            temp_constraints.emplace_back(constraint_sym->fq_name());
        }
        constraints->constraints = std::move(temp_constraints);
    }
}


auto spp::asts::GenericParameterTypeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the name.
    name->stage_7_analyse_semantics(sm, meta);
}
