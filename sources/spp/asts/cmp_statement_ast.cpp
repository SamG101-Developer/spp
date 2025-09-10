#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/mem_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/annotation_ast.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/cmp_statement_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/pch.hpp>

#include <genex/views/for_each.hpp>


spp::asts::CmpStatementAst::CmpStatementAst(
    decltype(annotations) &&annotations,
    decltype(tok_cmp) &&tok_cmp,
    decltype(name) &&name,
    decltype(tok_colon) &&tok_colon,
    decltype(type) type,
    decltype(tok_assign) &&tok_assign,
    decltype(value) &&value) :
    annotations(std::move(annotations)),
    tok_cmp(std::move(tok_cmp)),
    name(std::move(name)),
    tok_colon(std::move(tok_colon)),
    type(std::move(type)),
    tok_assign(std::move(tok_assign)),
    value(std::move(value)) {
}


spp::asts::CmpStatementAst::~CmpStatementAst() = default;


auto spp::asts::CmpStatementAst::pos_start() const -> std::size_t {
    return name->pos_start();
}


auto spp::asts::CmpStatementAst::pos_end() const -> std::size_t {
    return value->pos_end();
}


auto spp::asts::CmpStatementAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<CmpStatementAst>(
        ast_clone_vec(annotations),
        ast_clone(tok_cmp),
        ast_clone(name),
        ast_clone(tok_colon),
        ast_clone(type),
        ast_clone(tok_assign),
        ast_clone(value));
}


spp::asts::CmpStatementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations);
    SPP_STRING_APPEND(tok_cmp);
    SPP_STRING_APPEND(name);
    SPP_STRING_APPEND(tok_colon);
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(tok_assign);
    SPP_STRING_APPEND(value);
    SPP_STRING_END;
}


auto spp::asts::CmpStatementAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(annotations);
    SPP_PRINT_APPEND(tok_cmp);
    SPP_PRINT_APPEND(name);
    SPP_PRINT_APPEND(tok_colon);
    SPP_PRINT_APPEND(type);
    SPP_PRINT_APPEND(tok_assign);
    SPP_PRINT_APPEND(value);
    SPP_PRINT_END;
}


auto spp::asts::CmpStatementAst::stage_1_pre_process(
    Ast *)
    -> void {
    // No pre-processing needed for cmp statements.
    annotations | genex::views::for_each([this](auto &&x) { x->stage_1_pre_process(this); });
}


auto spp::asts::CmpStatementAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // No top-level scopes needed for cmp statements.
    annotations | genex::views::for_each([sm, meta](auto &&x) { x->stage_2_gen_top_level_scopes(sm, meta); });

    // Ensure that the convention type doesn't have a convention.
    if (const auto conv = type->get_convention(); conv != nullptr) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSecondClassBorrowViolationError>().with_args(
            *this, *conv, "global constant type").with_scopes({sm->current_scope}).raise();
    }

    // Create a symbol for this constant declaration.
    auto sym = std::make_unique<analyse::scopes::VariableSymbol>(name, type, false, false, m_visibility.first);
    sym->memory_info->ast_pins.emplace_back(name.get());
    sym->memory_info->ast_comptime = this;
    sym->memory_info->initialized_by(*this);
    sm->current_scope->add_var_symbol(std::move(sym));
}


auto spp::asts::CmpStatementAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the type exists before attaching super scopes
    type->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::CmpStatementAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the type and value.
    type->stage_7_analyse_semantics(sm, meta);
    value->stage_7_analyse_semantics(sm, meta);

    // Check the value's type is the same as the given type.
    const auto given_type = type.get();
    const auto inferred_type = value->infer_type(sm, meta);

    if (not analyse::utils::type_utils::symbolic_eq(*given_type, *inferred_type, *sm->current_scope, *sm->current_scope)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>().with_args(
            *type, *given_type, *value, *inferred_type).with_scopes({sm->current_scope}).raise();
    }
}


auto spp::asts::CmpStatementAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the memory of the type.
    value->stage_8_check_memory(sm, meta);
    analyse::utils::mem_utils::validate_symbol_memory(*value, *value, *sm, true, true, true, true, true, true, meta);
}
