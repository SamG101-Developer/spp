#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/asts/convention_mut_ast.hpp>
#include <spp/asts/convention_ref_ast.hpp>
#include <spp/asts/function_parameter_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/let_statement_uninitialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::FunctionParameterAst::FunctionParameterAst(
    decltype(var) &&var,
    decltype(tok_colon) &&tok_colon,
    decltype(type) &&type) :
    var(std::move(var)),
    tok_colon(std::move(tok_colon)),
    type(std::move(type)) {
}


spp::asts::FunctionParameterAst::~FunctionParameterAst() = default;


auto spp::asts::FunctionParameterAst::extract_names() const -> std::vector<std::shared_ptr<IdentifierAst>> {
    return var->extract_names();
}


auto spp::asts::FunctionParameterAst::extract_name() const -> std::shared_ptr<IdentifierAst> {
    return var->extract_name();
}


auto spp::asts::FunctionParameterAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the type.
    type->stage_7_analyse_semantics(sm, meta);
    type = sm->current_scope->get_type_symbol(*type)->fq_name();

    // Create the variable for the parameter (use temp copies and put them back).
    const auto ast = std::make_unique<LetStatementUninitializedAst>(nullptr, std::move(var), nullptr, std::move(type));
    ast->stage_7_analyse_semantics(sm, meta);
    var = std::move(ast->var);
    type = std::move(ast->type);

    // Mark the symbol as initialized.
    const auto conv = type->get_convention();
    for (auto &&name : extract_names()) {
        const auto sym = sm->current_scope->get_var_symbol(*name);
        sym->memory_info->initialized_by(*this);
        sym->memory_info->ast_borrowed = conv;
        sym->memory_info->is_borrow_mut = conv->tag == ConventionAst::ConventionTag::MUT;
        sym->memory_info->is_borrow_ref = conv->tag == ConventionAst::ConventionTag::REF;
    }
}


auto spp::asts::FunctionParameterAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *)
    -> void {
    // Check the memory of each name.
    for (auto &&name : extract_names()) {
        const auto sym = sm->current_scope->get_var_symbol(*name);
        sym->memory_info->initialized_by(*this);
    }
}
