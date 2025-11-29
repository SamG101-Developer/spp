module;
#include <genex/to_container.hpp>
#include <genex/views/for_each.hpp>
#include <genex/views/intersperse.hpp>
#include <genex/views/join.hpp>
#include <genex/views/transform.hpp>

#include <spp/macros.hpp>

module spp.asts.type_statement_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_block_name;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_utils;
import spp.asts.annotation_ast;
import spp.asts.convention_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;


spp::asts::TypeStatementAst::TypeStatementAst(
    decltype(annotations) &&annotations,
    decltype(tok_type) &&tok_type,
    decltype(new_type) new_type,
    decltype(generic_param_group) &&generic_param_group,
    decltype(tok_assign) &&tok_assign,
    decltype(old_type) old_type) :
    m_generated(false),
    m_for_use_statement(false),
    m_temp_scope_1(nullptr),
    m_temp_scope_2(nullptr),
    annotations(std::move(annotations)),
    tok_type(std::move(tok_type)),
    new_type(std::move(new_type)),
    generic_param_group(std::move(generic_param_group)),
    tok_assign(std::move(tok_assign)),
    old_type(std::move(old_type)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_type, lex::SppTokenType::KW_TYPE, "type");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->generic_param_group);
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_assign, lex::SppTokenType::TK_ASSIGN, "=");
}


spp::asts::TypeStatementAst::~TypeStatementAst() {
    // Because the TypeStatementAst is passed as a unique pointer to the TypeSymbol, we need to clear it from the type
    // symbol without destroying it, otherwise there is a use after free because of a double destruction; the unique
    // pointer destroying the type statement, then the type statement destroying itself (via the destructor). Releasing
    // it here prevents the type symbol from destroying it.
    if (m_type_symbol != nullptr) {
        m_type_symbol->alias_stmt.release();
    }

    // Now this pointer has been released from the type symbol, we can safely destroy the type statement.
}


auto spp::asts::TypeStatementAst::pos_start() const
    -> std::size_t {
    return tok_type->pos_start();
}


auto spp::asts::TypeStatementAst::pos_end() const
    -> std::size_t {
    return old_type->pos_end();
}


auto spp::asts::TypeStatementAst::clone() const
    -> std::unique_ptr<Ast> {
    auto ast = std::make_unique<TypeStatementAst>(
        ast_clone_vec(annotations),
        ast_clone(tok_type),
        ast_clone(new_type),
        ast_clone(generic_param_group),
        ast_clone(tok_assign),
        ast_clone(old_type));
    ast->m_ctx = m_ctx;
    ast->m_scope = m_scope;
    ast->m_temp_scope_1 = m_temp_scope_1;
    ast->m_visibility = m_visibility;
    ast->annotations | genex::views::for_each([ast=ast.get()](auto &&a) { a->m_ctx = ast; });
    return ast;
}


spp::asts::TypeStatementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations);
    SPP_STRING_APPEND(tok_type).append(" ");
    SPP_STRING_APPEND(new_type);
    SPP_STRING_APPEND(generic_param_group).append(" ");
    SPP_STRING_APPEND(tok_assign).append(" ");
    SPP_STRING_APPEND(old_type);
    SPP_STRING_END;
}


auto spp::asts::TypeStatementAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(annotations);
    SPP_PRINT_APPEND(tok_type).append(" ");
    SPP_PRINT_APPEND(new_type);
    SPP_PRINT_APPEND(generic_param_group).append(" ");
    SPP_PRINT_APPEND(tok_assign).append(" ");
    SPP_PRINT_APPEND(old_type);
    SPP_PRINT_END;
}


auto spp::asts::TypeStatementAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // Pre-process the annotations.
    Ast::stage_1_pre_process(ctx);
    annotations | genex::views::for_each([this](auto &&x) { x->stage_1_pre_process(this); });
}


auto spp::asts::TypeStatementAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Run top level scope generation for the annotations.
    Ast::stage_2_gen_top_level_scopes(sm, meta);
    annotations | genex::views::for_each([sm, meta](auto &&x) { x->stage_2_gen_top_level_scopes(sm, meta); });

    // Check there are no conventions on the old type.
    if (auto &&conv = old_type->get_convention(); conv != nullptr) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSecondClassBorrowViolationError>().with_args(
            *old_type, *conv, "use statement's old type").with_scopes({sm->current_scope}).raise();
    }

    // Check there are no conventions on the new type.
    if (auto &&conv = new_type->get_convention(); conv != nullptr) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppSecondClassBorrowViolationError>().with_args(
            *new_type, *conv, "use statement's new type").with_scopes({sm->current_scope}).raise();
    }

    // Create the type symbol for this type, that will point to the old type.
    m_type_symbol = std::make_shared<analyse::scopes::TypeSymbol>(
        new_type, nullptr, nullptr, sm->current_scope, sm->current_scope->parent_module());
    m_type_symbol->alias_stmt = std::unique_ptr<TypeStatementAst>(this);
    sm->current_scope->add_type_symbol(m_type_symbol);

    // Create a new scope for the type statement.
    auto scope_name = analyse::scopes::ScopeBlockName("<type-stmt#" + static_cast<std::string>(*new_type) + "#" + std::to_string(pos_start()) + ">");
    sm->create_and_move_into_new_scope(std::move(scope_name), this);
    Ast::stage_2_gen_top_level_scopes(sm, meta);
    generic_param_group->stage_2_gen_top_level_scopes(sm, meta);
    sm->move_out_of_current_scope();
    m_generated = true;
}


auto spp::asts::TypeStatementAst::stage_3_gen_top_level_aliases(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Skip the class scope, and enter the type statement scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);

    // Recursively discover the actual type being mapped to.
    auto [actual_old_type, attach_generics, scope1, scope2] = analyse::utils::type_utils::recursive_alias_search(
        *this, sm->current_scope->parent, old_type, sm, meta);
    m_temp_scope_1 = scope1;
    m_temp_scope_2 = scope2;

    const auto final_sym = sm->current_scope->get_type_symbol(actual_old_type->without_generics());
    m_type_symbol->type = final_sym->type;
    m_type_symbol->scope = final_sym->scope;
    m_type_symbol->is_copyable = [final_sym] { return final_sym->is_copyable(); };
    old_type = actual_old_type;

    if (attach_generics != nullptr and not attach_generics->params.empty()) {
        generic_param_group = attach_generics;
        generic_param_group->stage_2_gen_top_level_scopes(sm, meta);
    }

    sm->move_out_of_current_scope();
}


auto spp::asts::TypeStatementAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Skip the class scope, and enter the type statement scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);

    // Get the old type's symbol, without generics.
    const auto stripped_old_sym = sm->current_scope->get_type_symbol(old_type->without_generics(), false);
    if (not stripped_old_sym->is_generic) {
        auto tm_1 = ScopeManager(sm->global_scope, stripped_old_sym->scope);
        auto tm_2 = ScopeManager(sm->global_scope, m_temp_scope_1);

        auto temp_scope = std::make_unique<analyse::scopes::Scope>(*m_temp_scope_2->parent);
        auto tm_3 = ScopeManager(sm->global_scope, temp_scope.get());
        generic_param_group->stage_2_gen_top_level_scopes(&tm_3, meta);

        // Qualify the generics, and the overall type.
        // generic_param_group->stage_4_qualify_types(&tm_3, meta);
        old_type->stage_4_qualify_types(&tm_1, meta); // Extends generics into fq from the old symbols scope.
        old_type->stage_4_qualify_types(&tm_2, meta); // Extends generics into fq from the old symbols scope.
        old_type->stage_7_analyse_semantics(sm, meta); // Analyse the fq old type in this scope (for generics)

        const auto old_sym = sm->current_scope->get_type_symbol(old_type);
        m_type_symbol->type = old_sym->type;
        m_type_symbol->scope = old_sym->scope;
        m_temp_scope_3 = std::move(temp_scope);
    }
    sm->move_out_of_current_scope();
}


auto spp::asts::TypeStatementAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    sm->move_out_of_current_scope();
}


auto spp::asts::TypeStatementAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    sm->move_out_of_current_scope();
}


auto spp::asts::TypeStatementAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // If this is a pre-generated AST (mod/sup context), skip any generation steps.
    if (m_generated) {
        sm->move_to_next_scope();
        SPP_ASSERT(sm->current_scope == m_scope);
        sm->move_out_of_current_scope();
        return;
    }

    // Otherwise, run all generation steps.
    const auto current_scope = sm->current_scope;
    auto iter_copy = sm->m_it;

    sm->reset(current_scope, iter_copy);
    iter_copy = sm->m_it;
    stage_2_gen_top_level_scopes(sm, meta);

    sm->reset(current_scope, iter_copy);
    iter_copy = sm->m_it;
    stage_3_gen_top_level_aliases(sm, meta);

    sm->reset(current_scope, iter_copy);
    stage_4_qualify_types(sm, meta);
}


auto spp::asts::TypeStatementAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    sm->move_out_of_current_scope();
}
