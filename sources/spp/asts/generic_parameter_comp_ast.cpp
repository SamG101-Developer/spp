#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/generic_parameter_comp_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::GenericParameterCompAst::GenericParameterCompAst(
    decltype(tok_cmp) &&tok_cmp,
    decltype(name) name,
    decltype(tok_colon) &&tok_colon,
    decltype(type) type) :
    GenericParameterAst(std::move(name)),
    tok_cmp(std::move(tok_cmp)),
    tok_colon(std::move(tok_colon)),
    type(std::move(type)) {
}


spp::asts::GenericParameterCompAst::~GenericParameterCompAst() = default;


auto spp::asts::GenericParameterCompAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *)
    -> void {
    // Ensure the type does not have a convention.
    if (const auto conv = type->get_convention(); conv != nullptr) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSecondClassBorrowViolationError>().with_args(
            *type, *conv, "function return type").with_scopes({sm->current_scope}).raise();
    }

    // Create a variable symbol for this constant in the current scope (class / function).
    auto sym = std::make_unique<analyse::scopes::VariableSymbol>(
        IdentifierAst::from_type(*name), type, false, true, utils::Visibility::PUBLIC);
    sym->memory_info->ast_pins.emplace_back(name.get());
    sym->memory_info->ast_comptime = ast_clone(this);
    sym->memory_info->initialized_by(*this);
    sm->current_scope->add_var_symbol(std::move(sym));
}


auto spp::asts::GenericParameterCompAst::stage_4_qualify_types(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Qualify the type on the generic parameter (and in the symbol). Note: not possible to have a convention here.
    type->stage_7_analyse_semantics(sm, meta);
    type = sm->current_scope->get_type_symbol(type)->fq_name();
    sm->current_scope->get_var_symbol(IdentifierAst::from_type(*name))->type = type;
}


auto spp::asts::GenericParameterCompAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the type.
    type->stage_7_analyse_semantics(sm, meta);
}
