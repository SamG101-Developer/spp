#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/scopes/symbols.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/asts/convention_mut_ast.hpp>
#include <spp/asts/convention_ref_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/local_variable_single_identifier_alias_ast.hpp>
#include <spp/asts/local_variable_single_identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::LocalVariableSingleIdentifierAst::LocalVariableSingleIdentifierAst(
    decltype(tok_mut) &&tok_mut,
    decltype(name) &&name,
    decltype(alias) &&alias) :
    tok_mut(std::move(tok_mut)),
    name(std::move(name)),
    alias(std::move(alias)) {
}


spp::asts::LocalVariableSingleIdentifierAst::~LocalVariableSingleIdentifierAst() = default;


auto spp::asts::LocalVariableSingleIdentifierAst::pos_start() const -> std::size_t {
    return tok_mut ? tok_mut->pos_start() : name->pos_start();
}


auto spp::asts::LocalVariableSingleIdentifierAst::pos_end() const -> std::size_t {
    return alias->pos_end();
}


auto spp::asts::LocalVariableSingleIdentifierAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<LocalVariableSingleIdentifierAst>(
        ast_clone(tok_mut),
        ast_clone(name),
        ast_clone(alias));
}


spp::asts::LocalVariableSingleIdentifierAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_mut);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(alias);
    SPP_STRING_END;
}


auto spp::asts::LocalVariableSingleIdentifierAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_mut);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(alias);
    SPP_PRINT_END;
}


auto spp::asts::LocalVariableSingleIdentifierAst::extract_name() const
    -> std::shared_ptr<IdentifierAst> {
    return name;
}


auto spp::asts::LocalVariableSingleIdentifierAst::extract_names() const
    -> std::vector<std::shared_ptr<IdentifierAst>> {
    return {name};
}


auto spp::asts::LocalVariableSingleIdentifierAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Get the value and its type from the "meta" information.
    const auto val = meta->let_stmt_value;
    const auto val_type = val != nullptr ? val->infer_type(sm, meta) : nullptr;

    // Create a variable symbol for this identifier and value.
    auto sym = std::make_unique<analyse::scopes::VariableSymbol>(
        alias != nullptr ? alias->name : name,
        meta->let_stmt_explicit_type != nullptr ? meta->let_stmt_explicit_type : val_type,
        tok_mut != nullptr);

    // Set the initialization AST (for errors).
    sym->memory_info->ast_initialization = name.get();
    sym->memory_info->ast_initialization_origin = name.get();

    // Increment the initialization counter for initialized statements.
    if (val != nullptr) {
        sym->memory_info->initialization_counter = 1;

        // Set borrow asts based on the value's type's convention.
        if (const auto conv = val_type->get_convention(); conv != nullptr) {
            sym->memory_info->ast_borrowed = val;
            sym->memory_info->is_borrow_mut = *conv == ConventionAst::ConventionTag::MUT;
            sym->memory_info->is_borrow_ref = *conv == ConventionAst::ConventionTag::REF;
        }
    }

    else {
        // Mark an uninitialized variable as "moved"
        sym->memory_info->ast_moved = this;
    }

    // Add the symbol to the current scope.
    sm->current_scope->add_var_symbol(std::move(sym));
}


auto spp::asts::LocalVariableSingleIdentifierAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // No value => nothing to check.
    if (meta->let_stmt_value == nullptr) { return; }

    // Check the value's memory.
    meta->let_stmt_value->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(
        *meta->let_stmt_value, *this, *sm, true, true, true, true, false, true, meta);

    // Get the name or alias symbol to mark it as initialized.
    const auto sym = sm->current_scope->get_var_symbol(alias != nullptr ? alias->name : name);
    sym->memory_info->initialized_by(*name);
}
