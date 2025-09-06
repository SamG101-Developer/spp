#include <algorithm>

#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/array_literal_explicit_elements_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/generic_argument_type_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/integer_literal_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_destructure_attribute_binding_ast.hpp>
#include <spp/asts/local_variable_destructure_object_ast.hpp>
#include <spp/asts/local_variable_destructure_skip_multiple_arguments_ast.hpp>
#include <spp/asts/local_variable_destructure_skip_single_argument_ast.hpp>
#include <spp/asts/local_variable_single_identifier_ast.hpp>
#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/postfix_expression_operator_static_member_access_ast.hpp>
#include <spp/asts/postfix_expression_operator_runtime_member_access_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>

#include <genex/algorithms/count.hpp>
#include <genex/algorithms/position.hpp>
#include <genex/actions/concat.hpp>
#include <genex/views/cast.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/flatten.hpp>
#include <genex/views/for_each.hpp>
#include <genex/views/iota.hpp>
#include <genex/views/ptr.hpp>
#include <genex/views/to.hpp>
#include <genex/views/zip.hpp>


spp::asts::LocalVariableDestructureObjectAst::LocalVariableDestructureObjectAst(
    decltype(type) &&type,
    decltype(tok_l) &&tok_l,
    decltype(elems) &&elems,
    decltype(tok_r) &&tok_r) :
    type(std::move(type)),
    tok_l(std::move(tok_l)),
    elems(std::move(elems)),
    tok_r(std::move(tok_r)) {
}


spp::asts::LocalVariableDestructureObjectAst::~LocalVariableDestructureObjectAst() = default;


auto spp::asts::LocalVariableDestructureObjectAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::LocalVariableDestructureObjectAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::LocalVariableDestructureObjectAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<LocalVariableDestructureObjectAst>(
        ast_clone(type),
        ast_clone(tok_l),
        ast_clone_vec(elems),
        ast_clone(tok_r));
}


spp::asts::LocalVariableDestructureObjectAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(type);
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(elems);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::LocalVariableDestructureObjectAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(type);
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(elems);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


auto spp::asts::LocalVariableDestructureObjectAst::extract_name() const
    -> std::shared_ptr<IdentifierAst> {
    return std::make_shared<IdentifierAst>(pos_start(), "_UNMATCHABLE");
}


auto spp::asts::LocalVariableDestructureObjectAst::extract_names() const
    -> std::vector<std::shared_ptr<IdentifierAst>> {
    return elems
        | genex::views::transform(&LocalVariableAst::extract_names)
        | genex::views::flatten
        | genex::views::to<std::vector>();
}


auto spp::asts::LocalVariableDestructureObjectAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Only 1 "multi-skip" allowed in a destructure.
    const auto multi_arg_skips = elems
        | genex::views::ptr
        | genex::views::cast_dynamic<LocalVariableDestructureSkipMultipleArgumentsAst*>()
        | genex::views::filter([](auto &&x) { return x != nullptr; })
        | genex::views::to<std::vector>();

    if (multi_arg_skips.size() > 1) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppMultipleSkipMultiArgumentsError>().with_args(
            *this, *multi_arg_skips[0], *multi_arg_skips[1]).with_scopes({sm->current_scope}).raise();
    }

    // Multi skips cannot be bound.
    if (not multi_arg_skips.empty() and multi_arg_skips[0]->binding != nullptr) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppVariableObjectDestructureWithBoundMultiSkipError>().with_args(
            *this, *multi_arg_skips[0]).with_scopes({sm->current_scope}).raise();
    }

    const auto val = meta->let_stmt_value;
    const auto val_type = val->infer_type(sm, meta)->type_parts().back();

    // Create expanded "let" statements for each part of the destructure.
    for (auto elem : elems | genex::views::ptr) {
        // Skip any conversion for unbound multi argument skipping.
        if (ast_cast<LocalVariableDestructureSkipMultipleArgumentsAst>(elem) != nullptr) {
        }

        // Skip any conversion for single argument skipping.
        else if (const auto cast_elem_1 = ast_cast<LocalVariableSingleIdentifierAst>(elem); cast_elem_1 != nullptr) {
            auto field = std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, ast_clone(cast_elem_1->name));
            auto pstfx = std::make_unique<PostfixExpressionAst>(ast_clone(val), std::move(field));
            auto new_ast = std::make_unique<LetStatementInitializedAst>(nullptr, ast_clone(elem), nullptr, nullptr, std::move(pstfx));
            new_ast->stage_7_analyse_semantics(sm, meta);
            m_new_asts.emplace_back(std::move(new_ast));
        }

        // Skip any conversion for single argument skipping.
        else if (ast_cast<LocalVariableDestructureSkipSingleArgumentAst>(elem) != nullptr) {
        }

        // Handle and other nested destructure or single identifier.
        else if (const auto cast_elem_2 = ast_cast<LocalVariableDestructureAttributeBindingAst>(elem); cast_elem_2 != nullptr) {
            auto field = std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, ast_clone(cast_elem_2->name));
            auto pstfx = std::make_unique<PostfixExpressionAst>(ast_clone(val), std::move(field));
            auto new_ast = std::make_unique<LetStatementInitializedAst>(nullptr, ast_clone(cast_elem_2->val), nullptr, nullptr, std::move(pstfx));
            new_ast->stage_7_analyse_semantics(sm, meta);
            m_new_asts.emplace_back(std::move(new_ast));
        }
    }
}


auto spp::asts::LocalVariableDestructureObjectAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the memory state of the elements.
    m_new_asts | genex::views::for_each([sm, meta](auto &&x) { x->stage_8_check_memory(sm, meta); });
}
