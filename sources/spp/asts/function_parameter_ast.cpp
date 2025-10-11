#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/asts/convention_mut_ast.hpp>
#include <spp/asts/convention_ref_ast.hpp>
#include <spp/asts/function_parameter_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/let_statement_uninitialized_ast.hpp>
#include <spp/asts/local_variable_single_identifier_alias_ast.hpp>
#include <spp/asts/local_variable_single_identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::FunctionParameterAst::FunctionParameterAst(
    decltype(var) &&var,
    decltype(tok_colon) &&tok_colon,
    decltype(type) type,
    const decltype(m_order_tag) order_tag) :
    OrderableAst(order_tag),
    var(std::move(var)),
    tok_colon(std::move(tok_colon)),
    type(std::move(type)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_colon, lex::SppTokenType::TK_COLON, ":", var ? var->pos_end() : 0);
    if (this->var == nullptr) {
        auto var_name = std::make_unique<IdentifierAst>(0, std::format("$_{}", reinterpret_cast<std::uintptr_t>(this)));
        this->var = std::make_unique<LocalVariableSingleIdentifierAst>(nullptr, std::move(var_name), nullptr);
    }
}


spp::asts::FunctionParameterAst::~FunctionParameterAst() = default;


auto spp::asts::FunctionParameterAst::extract_names() const
    -> std::vector<std::shared_ptr<IdentifierAst>> {
    return var->extract_names();
}


auto spp::asts::FunctionParameterAst::extract_name() const
    -> std::shared_ptr<IdentifierAst> {
    return var->extract_name();
}


auto spp::asts::FunctionParameterAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the type.
    type->stage_7_analyse_semantics(sm, meta);
    type = sm->current_scope->get_type_symbol(type)->fq_name()->with_convention(ast_clone(type->get_convention()));

    // Create the variable for the parameter (use temp copies and put them back).
    const auto ast = std::make_unique<LetStatementUninitializedAst>(nullptr, std::move(var), nullptr, type);
    ast->stage_7_analyse_semantics(sm, meta);
    var = std::move(ast->var);

    // Mark the symbol as initialized.
    const auto conv = type->get_convention();
    for (auto const &name : extract_names()) {
        const auto sym = sm->current_scope->get_var_symbol(name);
        sym->memory_info->initialized_by(*this);
        sym->memory_info->ast_borrowed = conv;
        sym->memory_info->is_borrow_mut = conv and *conv == ConventionAst::ConventionTag::MUT;
        sym->memory_info->is_borrow_ref = conv and *conv == ConventionAst::ConventionTag::REF;
    }
}


auto spp::asts::FunctionParameterAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *)
    -> void {
    // Check the memory of each name.
    for (auto &&name : extract_names()) {
        const auto sym = sm->current_scope->get_var_symbol(name);
        sym->memory_info->initialized_by(*this);
    }
}
