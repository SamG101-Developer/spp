module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.type_statement_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
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
import genex;


spp::asts::TypeStatementAst::TypeStatementAst(
    decltype(annotations) &&annotations,
    decltype(tok_type) &&tok_type,
    decltype(new_type) new_type,
    decltype(generic_param_group) &&generic_param_group,
    decltype(tok_assign) &&tok_assign,
    decltype(old_type) old_type) :
    m_generated(false),
    m_from_use_statement(false),
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
    ast->visibility = visibility;
    for (auto const &a : ast->annotations) {
        a->set_ast_ctx(ast.get());
    }
    return ast;
}


spp::asts::TypeStatementAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(annotations, "\n").append(not annotations.empty() ? "\n" : "");
    SPP_STRING_APPEND(tok_type).append(" ");
    SPP_STRING_APPEND(new_type);
    SPP_STRING_APPEND(generic_param_group).append(" ");
    SPP_STRING_APPEND(tok_assign).append(" ");
    SPP_STRING_APPEND(old_type);
    SPP_STRING_END;
}


auto spp::asts::TypeStatementAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // Pre-process the annotations.
    Ast::stage_1_pre_process(ctx);
    for (auto const &annotation : annotations) {
        annotation->stage_1_pre_process(this);
    }
}


auto spp::asts::TypeStatementAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Run top level scope generation for the annotations.
    for (auto const &annotation : annotations) {
        annotation->stage_2_gen_top_level_scopes(sm, meta);
    }

    // Check there are no conventions on the new type.
    SPP_ENFORCE_SECOND_CLASS_BORROW_VIOLATION(
        new_type, new_type, *sm, "use statement's new type", false);

    // Create the type symbol for this type, that will point to the old type.
    const auto type_sym = std::make_shared<analyse::scopes::TypeSymbol>(
        new_type, nullptr, nullptr, sm->current_scope, sm->current_scope->parent_module());
    sm->current_scope->add_type_symbol(type_sym);
    m_alias_sym = type_sym;
    m_alias_sym->alias_stmt = std::unique_ptr<TypeStatementAst>(this);  // This is BAD but "cleanup" handles mem error.

    // Create a new scope for the type statement.
    auto scope_name = analyse::scopes::ScopeBlockName::from_parts(
        "type-stmt", {new_type.get()}, pos_start());
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
    auto [actual_old_type, attach_generics] = analyse::utils::type_utils::recursive_alias_search(
        *this, m_from_use_statement, sm->current_scope->parent, sm, meta);

    const auto final_sym = sm->current_scope->get_type_symbol(actual_old_type->without_generics());
    m_alias_sym->type = final_sym->type;
    m_alias_sym->scope = final_sym->scope;
    m_alias_sym->is_copyable = [final_sym] { return final_sym->is_copyable(); };
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
        generic_param_group->stage_4_qualify_types(sm, meta);
        old_type->stage_4_qualify_types(sm, meta);
        old_type->stage_7_analyse_semantics(sm, meta);

        const auto old_sym = sm->current_scope->get_type_symbol(old_type);
        m_alias_sym->type = old_sym->type;
        m_alias_sym->scope = old_sym->scope;
        old_sym->aliased_by_symbols.push_back(m_alias_sym);
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
        old_type->stage_7_analyse_semantics(sm, meta);
        sm->move_out_of_current_scope();
        return;
    }

    // Otherwise, run all generation steps.
    const auto current_scope = sm->current_scope;
    auto iter_copy = sm->current_iterator();

    sm->reset(current_scope, iter_copy);
    iter_copy = sm->current_iterator();
    stage_2_gen_top_level_scopes(sm, meta);

    sm->reset(current_scope, iter_copy);
    iter_copy = sm->current_iterator();
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


auto spp::asts::TypeStatementAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    sm->move_out_of_current_scope();
}


auto spp::asts::TypeStatementAst::stage_10_code_gen_1(
    ScopeManager *sm,
    CompilerMetaData *,
    codegen::LLvmCtx *)
    -> llvm::Value* {
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);
    sm->move_out_of_current_scope();
    return nullptr;
}


auto spp::asts::TypeStatementAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *,
    codegen::LLvmCtx *)
    -> llvm::Value* {
    sm->move_to_next_scope();
    // SPP_ASSERT(sm->current_scope == m_scope);
    sm->move_out_of_current_scope();
    return nullptr;
}


auto spp::asts::TypeStatementAst::mark_from_use_statement()
    -> void {
    m_from_use_statement = true;
}


auto spp::asts::TypeStatementAst::is_from_use_statement() const
    -> bool {
    return m_from_use_statement;
}


auto spp::asts::TypeStatementAst::cleanup() const
    -> void {
    // Because the TypeStatementAst is passed as a unique pointer to the TypeSymbol, we need to clear it from the type
    // symbol without destroying it, otherwise there is a use after free because of a double destruction; the unique
    // pointer destroying the type statement, then the type statement destroying itself (via the destructor). Releasing
    // it here prevents the type symbol from destroying it.
    if (m_alias_sym != nullptr) {
        m_alias_sym->alias_stmt.release();
    }

    // Now this pointer has been released from the type symbol, we can safely destroy the type statement.
}
