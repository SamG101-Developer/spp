module spp.asts.generic_parameter_type_ast;
import spp.analyse.scopes.scope;
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
    const decltype(m_order_tag) order_tag) :
    GenericParameterAst(std::move(name), order_tag),
    constraints(std::move(constraints)) {
}


spp::asts::GenericParameterTypeAst::~GenericParameterTypeAst() = default;


auto spp::asts::GenericParameterTypeAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    // Create the type symbol for the generic parameter.
    auto sym = std::make_unique<analyse::scopes::TypeSymbol>(
        ast_clone(name->type_parts().back().get()), nullptr, nullptr, sm->current_scope, nullptr, true);
    sm->current_scope->add_type_symbol(std::move(sym));
}


auto spp::asts::GenericParameterTypeAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse the name.
    name->stage_7_analyse_semantics(sm, meta);
}
