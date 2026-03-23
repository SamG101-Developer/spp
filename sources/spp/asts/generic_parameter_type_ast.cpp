module;
#include <spp/macros.hpp>

module spp.asts.generic_parameter_type_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.annotation_ast;
import spp.asts.class_prototype_ast;
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
    // Create a dummy scope for the generic type.
    auto dummy_scope_name = analyse::scopes::ScopeBlockName::from_parts(
        "generic-parameter-type", {name->type_parts().back().get()}, pos_start());
    m_dummy_ast = std::make_unique<ClassPrototypeAst>(SPP_NO_ANNOTATIONS, nullptr, nullptr, nullptr, nullptr);
    auto dummy_scope = std::make_unique<analyse::scopes::Scope>(
        dummy_scope_name, sm->current_scope, m_dummy_ast.get());

    // Create the type symbol for the generic parameter.
    const auto sym = std::make_shared<analyse::scopes::TypeSymbol>(
        ast_clone(name->type_parts().back().get()), nullptr, dummy_scope.get(), sm->current_scope, nullptr, true);
    sm->current_scope->add_type_symbol(sym);
    dummy_scope->ty_sym = sym;

    m_dummy_scopes.emplace_back(dummy_scope.get());
    ScopeManager::temp_scopes.emplace_back(std::move(dummy_scope));
}


auto spp::asts::GenericParameterTypeAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Qualify the name.
    name->stage_4_qualify_types(sm, nullptr);
    if (constraints != nullptr) {
        constraints->stage_7_analyse_semantics(sm, meta);

        // Attach the scopes of the cosntraint types as sup-scopes to the generic scope.
        for (auto const &constraint : constraints->constraints) {
            auto constraint_scope = sm->current_scope->get_type_symbol(constraint)->scope;

            for (auto const &dummy_scope : m_dummy_scopes) {
                dummy_scope->direct_sup_scopes.emplace_back(constraint_scope);
            }
        }
    }
}


auto spp::asts::GenericParameterTypeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the name.
    name->stage_7_analyse_semantics(sm, meta);
}
