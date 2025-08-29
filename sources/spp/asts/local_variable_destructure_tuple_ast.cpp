#include <algorithm>

#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/generic_argument_type_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/integer_literal_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_destructure_skip_multiple_arguments_ast.hpp>
#include <spp/asts/local_variable_destructure_skip_single_argument_ast.hpp>
#include <spp/asts/local_variable_destructure_tuple_ast.hpp>
#include <spp/asts/local_variable_single_identifier_ast.hpp>
#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/postfix_expression_operator_static_member_access_ast.hpp>
#include <spp/asts/postfix_expression_operator_runtime_member_access_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/tuple_literal_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>

#include <genex/algorithms/count.hpp>
#include <genex/algorithms/position.hpp>
#include <genex/views/cast.hpp>
#include <genex/views/concat.hpp>
#include <genex/views/filter.hpp>
#include <genex/views/flatten.hpp>
#include <genex/views/for_each.hpp>
#include <genex/views/iota.hpp>
#include <genex/views/materialize.hpp>
#include <genex/views/ptr.hpp>
#include <genex/views/to.hpp>
#include <genex/views/zip.hpp>


spp::asts::LocalVariableDestructureTupleAst::LocalVariableDestructureTupleAst(
    decltype(tok_l) &&tok_l,
    decltype(elems) &&elems,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    elems(std::move(elems)),
    tok_r(std::move(tok_r)) {
}


spp::asts::LocalVariableDestructureTupleAst::~LocalVariableDestructureTupleAst() = default;


auto spp::asts::LocalVariableDestructureTupleAst::pos_start() const -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::LocalVariableDestructureTupleAst::pos_end() const -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::LocalVariableDestructureTupleAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<LocalVariableDestructureTupleAst>(
        ast_clone(tok_l),
        ast_clone_vec(elems),
        ast_clone(tok_r));
}


spp::asts::LocalVariableDestructureTupleAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(elems);
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::LocalVariableDestructureTupleAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_l);
    SPP_PRINT_EXTEND(elems);
    SPP_PRINT_APPEND(tok_r);
    SPP_PRINT_END;
}


auto spp::asts::LocalVariableDestructureTupleAst::extract_name() const
    -> std::shared_ptr<IdentifierAst> {
    return std::make_shared<IdentifierAst>(pos_start(), "_UNMATCHABLE");
}


auto spp::asts::LocalVariableDestructureTupleAst::extract_names() const
    -> std::vector<std::shared_ptr<IdentifierAst>> {
    return elems
        | genex::views::map(&LocalVariableAst::extract_names)
        | genex::views::flatten
        | genex::views::to<std::vector>();
}


auto spp::asts::LocalVariableDestructureTupleAst::stage_7_analyse_semantics(
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
        analyse::errors::SppMultipleSkipMultiArgumentsError(*this, *multi_arg_skips[0], *multi_arg_skips[1])
            .scopes({sm->current_scope}).raise();
    }

    // Ensure the right-hand-side is an tuple type.
    const auto val = meta->let_stmt_value;
    const auto val_type = val->infer_type(sm, meta)->type_parts().back();
    if (not analyse::utils::type_utils::is_type_tuple(*val_type, *sm->current_scope)) {
        analyse::errors::SppVariableTupleDestructureTupleTypeMismatchError(*this, *val, *val_type)
            .scopes({sm->current_scope}).raise();
    }

    // Determine number of elements in the left-hand-side and right-hand-side tuples.
    const auto num_lhs_arr_elems = elems.size();
    const auto num_rhs_arr_elems = val->infer_type(sm, meta)->type_parts().back()->generic_arg_group->args.size();
    if ((num_lhs_arr_elems < num_rhs_arr_elems and multi_arg_skips.empty()) or (num_lhs_arr_elems > num_rhs_arr_elems)) {
        analyse::errors::SppVariableTupleDestructureTupleSizeMismatchError(*this, num_lhs_arr_elems, *val, num_rhs_arr_elems)
            .scopes({sm->current_scope}).raise();
    }

    // For a bound ".." destructure, ie "let [a, ..b, c] = t", create an intermediary type.
    auto bound_multi_skip = std::unique_ptr<TupleLiteralAst>(nullptr);
    if (not multi_arg_skips.empty() and multi_arg_skips[0]->binding != nullptr) {
        const auto m = elems | genex::views::ptr | genex::algorithms::position([&multi_arg_skips](auto &&x) { return x == multi_arg_skips[0]; });
        auto new_elems = genex::views::iota(m, m + num_rhs_arr_elems - num_lhs_arr_elems + 1)
            | genex::views::map([val](const auto i) {
                auto identifier = std::make_unique<IdentifierAst>(0uz, std::to_string(i));
                auto field = std::make_unique<PostfixExpressionOperatorStaticMemberAccessAst>(nullptr, std::move(identifier));
                auto postfix = std::make_unique<PostfixExpressionAst>(ast_clone(val), std::move(field));
                return postfix;
            })
            | genex::views::cast_smart_ptr<ExpressionAst>()
            | genex::views::to<std::vector>();

        bound_multi_skip = std::make_unique<TupleLiteralAst>(nullptr, std::move(new_elems), nullptr);
    }

    // Create new indexes.
    auto indexes = genex::views::iota(0, multi_arg_skips.empty() ? multi_arg_skips.size() : (elems | genex::views::ptr | genex::algorithms::position([&multi_arg_skips](auto &&x) { return x == multi_arg_skips[0]; })) + 1)
        | genex::views::concat(genex::views::iota(num_lhs_arr_elems, num_rhs_arr_elems) | genex::views::materialize);

    // Create expanded "let" statements for each part of the destructure.
    for (auto &&[i, elem] : genex::views::zip(indexes, elems | genex::views::ptr)) {
        const auto cast_elem = ast_cast<LocalVariableDestructureSkipMultipleArgumentsAst>(elem);

        // Handle bound multi argument skipping, by assigning the skipped elements into a variable.
        if (cast_elem != nullptr and multi_arg_skips[0]->binding != nullptr) {
            auto new_ast = std::make_unique<LetStatementInitializedAst>(nullptr, ast_clone(cast_elem->binding), nullptr, nullptr, std::move(bound_multi_skip));
            new_ast->stage_7_analyse_semantics(sm, meta);
            m_new_asts.emplace_back(std::move(new_ast));
        }

        // Skip any conversion for single argument skipping.
        else if (ast_cast<LocalVariableDestructureSkipSingleArgumentAst>(elem) != nullptr) {
        }

        // Skip any conversion for unbound multi argument skipping.
        else if (ast_cast<LocalVariableDestructureSkipMultipleArgumentsAst>(elem) != nullptr) {
        }

        // Handle and other nested destructure or single identifier.
        else {
            auto index = std::make_unique<IdentifierAst>(0uz, std::to_string(i));
            auto field = std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(index));
            auto pstfx = std::make_unique<PostfixExpressionAst>(ast_clone(val), std::move(field));
            auto new_ast = std::make_unique<LetStatementInitializedAst>(nullptr, ast_clone(elem), nullptr, nullptr, std::move(pstfx));
            new_ast->stage_7_analyse_semantics(sm, meta);
            m_new_asts.emplace_back(std::move(new_ast));
        }
    }
}


auto spp::asts::LocalVariableDestructureTupleAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the memory state of the elements.
    m_new_asts | genex::views::for_each([sm, meta](auto &&x) { x->stage_8_check_memory(sm, meta); });
}
