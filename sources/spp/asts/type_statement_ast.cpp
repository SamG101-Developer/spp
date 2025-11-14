#include <spp/pch.hpp>
#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/annotation_ast.hpp>
#include <spp/asts/class_implementation_ast.hpp>
#include <spp/asts/class_member_ast.hpp>
#include <spp/asts/class_prototype_ast.hpp>
#include <spp/asts/convention_ast.hpp>
#include <spp/asts/generic_argument_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/generic_parameter_ast.hpp>
#include <spp/asts/generic_parameter_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/sup_implementation_ast.hpp>
#include <spp/asts/sup_prototype_extension_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/asts/type_statement_ast.hpp>

#include <genex/views/for_each.hpp>


spp::asts::TypeStatementAst::TypeStatementAst(
    decltype(annotations) &&annotations,
    decltype(tok_type) &&tok_type,
    decltype(new_type) new_type,
    decltype(generic_param_group) &&generic_param_group,
    decltype(tok_assign) &&tok_assign,
    decltype(old_type) old_type) :
    m_generated(false),
    m_for_use_statement(false),
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


spp::asts::TypeStatementAst::~TypeStatementAst() = default;


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
    meta::AstPrinter &printer) const
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
    mixins::CompilerMetaData *meta)
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
    mixins::CompilerMetaData *meta)
    -> void {
    // Skip the class scope, and enter the type statement scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);

    // Recursively discover the actual type being mapped to. For example, if we have:
    // - use std::number::S32
    // - type A = B
    // - type B = C
    // - type C = S32
    // Then for A, want to find S32. This is done by following the old_type chain until we reach a type whose type
    // symbol has a "->type" that is not nullptr. This allows statements to be order-agnostic.

    auto [actual_old_type, attach_generics, scope] = analyse::utils::type_utils::recursive_alias_search(
        sm->current_scope->parent, old_type, sm, meta);
    m_temp_scope = scope;

    const auto final_sym = sm->current_scope->get_type_symbol(actual_old_type->without_generics());
    m_type_symbol->type = final_sym->type;
    m_type_symbol->scope = sm->current_scope;  // final_sym->scope?
    m_type_symbol->scope->ast = final_sym->scope ? final_sym->scope->ast : nullptr;  // allow for generics (no scope => no ast)
    old_type = actual_old_type;

    if (attach_generics != nullptr and not attach_generics->params.empty()) {
        generic_param_group = attach_generics;
        // old_type->type_parts().back()->generic_arg_group->args = std::move(GenericArgumentGroupAst::from_params(*attach_generics)->args);
        generic_param_group->stage_2_gen_top_level_scopes(sm, meta);
    }

    // std::cout << operator std::string() << std::endl;
    // std::cout << "\n" << operator std::string() << std::endl;

    // // For "use" statements, extra processing.
    // if (m_for_use_statement) {
    //     const auto old_type_sym = sm->current_scope->get_type_symbol(old_type->without_generics());
    //     const auto old_type_type = analyse::utils::type_utils::recursive_alias_search(old_type_sym->scope_defined_in, old_type, sm, meta);
    //     const auto generic_params = sm->current_scope->get_type_symbol(old_type_type)->type->generic_param_group;
    //     old_type = actual_old_type;
    //     generic_param_group = generic_params;
    //     old_type->type_parts().back()->generic_arg_group = GenericArgumentGroupAst::from_params(*generic_params);
    //     generic_param_group->stage_2_gen_top_level_scopes(sm, meta);
    // }

    sm->move_out_of_current_scope();

    // Analyse the old type without generics, to ensure the base type exists.
    // meta->save();
    // meta->skip_type_analysis_generic_checks = true;
    // old_type->without_generics()->stage_7_analyse_semantics(sm, meta);
    // meta->restore();

    //todo: move the generic param generation from stage 2 to this stage (use statememt ready now)

    // Check the (full) old type is valid, and get the new symbol.
    // old_type->stage_7_analyse_semantics(sm, meta);
}


auto spp::asts::TypeStatementAst::stage_4_qualify_types(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Skip the class scope, and enter the type statement scope.
    sm->move_to_next_scope();
    SPP_ASSERT(sm->current_scope == m_scope);

    // Get the old type's symbol, without generics.
    const auto stripped_old_sym = sm->current_scope->get_type_symbol(old_type->without_generics(), false);
    if (not stripped_old_sym->is_generic) {
        auto tm_1 = ScopeManager(sm->global_scope, stripped_old_sym->scope);
        auto tm_2 = ScopeManager(sm->global_scope, m_temp_scope);

        // if (m_for_use_statement and generic_param_group->params.empty()) {
        //     const auto old_type_sym = sm->current_scope->get_type_symbol(m_original_old_type);
        //     const auto generic_params = old_type_sym->type->generic_param_group;
        //
        //     // Add the generic parameters to the conversion AST, and add mock generic arguments to the old type.
        //     generic_param_group = generic_params;
        //     old_type->type_parts().back()->generic_arg_group->args = std::move(GenericArgumentGroupAst::from_params(*generic_params)->args);
        //     generic_param_group->stage_2_gen_top_level_scopes(sm, meta);
        // }

        // Qualify the generics, and the overall type.
        generic_param_group->stage_4_qualify_types(&tm_1, meta);
        old_type->stage_4_qualify_types(&tm_1, meta); // Extends generics into fq from the old symbols scope.
        old_type->stage_4_qualify_types(&tm_2, meta); // Extends generics into fq from the old symbols scope.
        old_type->stage_7_analyse_semantics(sm, meta); // Analyse the fq old type in this scope (for generics)
    }
    sm->move_out_of_current_scope();
}


auto spp::asts::TypeStatementAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *) -> void {
    sm->move_to_next_scope();
    sm->move_out_of_current_scope();
}


auto spp::asts::TypeStatementAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *) -> void {
    sm->move_to_next_scope();
    sm->move_out_of_current_scope();
}


auto spp::asts::TypeStatementAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // If this is a pre-generated AST (mod/sup context), skip any generation steps.
    if (m_generated) {
        sm->move_to_next_scope();
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
    mixins::CompilerMetaData *) -> void {
    sm->move_to_next_scope();
    sm->move_out_of_current_scope();
}
