module;
#include <opex/macros.hpp>
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.local_variable_destructure_tuple_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.type_utils;
import spp.asts.expression_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_destructure_skip_multiple_arguments_ast;
import spp.asts.local_variable_destructure_skip_single_argument_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.tuple_literal_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.lex.tokens;
import genex;
import opex.cast;


spp::asts::LocalVariableDestructureTupleAst::LocalVariableDestructureTupleAst(
    decltype(tok_l) &&tok_l,
    decltype(elems) &&elems,
    decltype(tok_r) &&tok_r) :
    tok_l(std::move(tok_l)),
    elems(std::move(elems)),
    tok_r(std::move(tok_r)) {
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_l, lex::SppTokenType::TK_LEFT_PARENTHESIS, "(");
    SPP_SET_AST_TO_DEFAULT_IF_NULLPTR(this->tok_r, lex::SppTokenType::TK_RIGHT_PARENTHESIS, ")");
}


spp::asts::LocalVariableDestructureTupleAst::~LocalVariableDestructureTupleAst() = default;


auto spp::asts::LocalVariableDestructureTupleAst::pos_start() const
    -> std::size_t {
    return tok_l->pos_start();
}


auto spp::asts::LocalVariableDestructureTupleAst::pos_end() const
    -> std::size_t {
    return tok_r->pos_end();
}


auto spp::asts::LocalVariableDestructureTupleAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<LocalVariableDestructureTupleAst>(
        ast_clone(tok_l),
        ast_clone_vec(elems),
        ast_clone(tok_r));
}


spp::asts::LocalVariableDestructureTupleAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_l);
    SPP_STRING_EXTEND(elems, ", ");
    SPP_STRING_APPEND(tok_r);
    SPP_STRING_END;
}


auto spp::asts::LocalVariableDestructureTupleAst::extract_name() const
    -> std::shared_ptr<IdentifierAst> {
    return std::make_shared<IdentifierAst>(pos_start(), "_UNMATCHABLE");
}


auto spp::asts::LocalVariableDestructureTupleAst::extract_names() const
    -> std::vector<std::shared_ptr<IdentifierAst>> {
    return elems
        | genex::views::transform(&LocalVariableAst::extract_names)
        | genex::views::join
        | genex::to<std::vector>();
}


auto spp::asts::LocalVariableDestructureTupleAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Only 1 "multi-skip" allowed in a destructure.
    const auto multi_arg_skips = elems
        | genex::views::ptr
        | genex::views::cast_dynamic<LocalVariableDestructureSkipMultipleArgumentsAst*>()
        | genex::to<std::vector>();

    if (multi_arg_skips.size() > 1) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppMultipleSkipMultiArgumentsError>()
            .with_args(*this, *multi_arg_skips[0], *multi_arg_skips[1])
            .raises_from(sm->current_scope);
    }

    // Ensure the right-hand-side is a tuple type.
    const auto val = meta->let_stmt_value;
    const auto val_type = val->infer_type(sm, meta);
    if (not analyse::utils::type_utils::is_type_tuple(*val_type, *sm->current_scope)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppVariableTupleDestructureTupleTypeMismatchError>()
            .with_args(*this, *val, *val_type)
            .raises_from(sm->current_scope);
    }

    // Determine number of elements in the left-hand-side and right-hand-side tuples.
    const auto num_lhs_arr_elems = elems.size();
    const auto num_rhs_arr_elems = val->infer_type(sm, meta)->type_parts().back()->generic_arg_group->args.size();
    if ((num_lhs_arr_elems < num_rhs_arr_elems and multi_arg_skips.empty()) or (num_lhs_arr_elems > num_rhs_arr_elems)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppVariableTupleDestructureTupleSizeMismatchError>()
            .with_args(*this, num_lhs_arr_elems, *val, num_rhs_arr_elems)
            .raises_from(sm->current_scope);
    }

    // For a bound ".." destructure, ie "let [a, ..b, c] = t", create an intermediary type.
    auto bound_multi_skip = std::unique_ptr<TupleLiteralAst>(nullptr);
    if (not multi_arg_skips.empty() and multi_arg_skips[0]->binding != nullptr) {
        const auto m = genex::position(elems | genex::views::ptr, [&multi_arg_skips](auto &&x) { return x == multi_arg_skips[0]; }) as USize;
        auto new_elems = genex::views::iota(m, m + num_rhs_arr_elems - num_lhs_arr_elems + 1)
            | genex::views::transform([val](const auto i) {
                auto identifier = std::make_unique<IdentifierAst>(0uz, std::to_string(i));
                auto field = std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(identifier));
                auto postfix = std::make_unique<PostfixExpressionAst>(ast_clone(val), std::move(field));
                return postfix;
            })
            | genex::views::cast_smart<ExpressionAst>()
            | genex::to<std::vector>();

        bound_multi_skip = std::make_unique<TupleLiteralAst>(nullptr, std::move(new_elems), nullptr);
    }

    // Create new indexes.
    const auto skip_index = not multi_arg_skips.empty()
                                ? genex::position(elems | genex::views::ptr, [&multi_arg_skips](auto &&x) { return x == multi_arg_skips[0]; }) as USize
                                : elems.size() - 1;
    auto indexes = genex::views::iota(0uz, skip_index + 1uz) | genex::to<std::vector>();
    indexes.append_range(genex::views::iota(num_lhs_arr_elems, num_rhs_arr_elems) | genex::to<std::vector>());

    // Create expanded "let" statements for each part of the destructure.
    for (auto &&[i, elem] : genex::views::zip(indexes, elems | genex::views::ptr)) {
        const auto cast_elem = elem->to<LocalVariableDestructureSkipMultipleArgumentsAst>();

        // Handle bound multi argument skipping, by assigning the skipped elements into a variable.
        if (cast_elem != nullptr and multi_arg_skips[0]->binding != nullptr) {
            auto new_ast = std::make_unique<LetStatementInitializedAst>(nullptr, ast_clone(cast_elem->binding), nullptr, nullptr, std::move(bound_multi_skip));
            if (m_from_case_pattern) { new_ast->var->mark_from_case_pattern(); }
            new_ast->stage_7_analyse_semantics(sm, meta);
            m_new_asts.emplace_back(std::move(new_ast));
        }

        // Skip any conversion for single argument skipping.
        else if (elem->to<LocalVariableDestructureSkipSingleArgumentAst>() != nullptr) {
        }

        // Skip any conversion for unbound multi argument skipping.
        else if (elem->to<LocalVariableDestructureSkipMultipleArgumentsAst>() != nullptr) {
        }

        // Handle and other nested destructure or single identifier.
        else {
            auto index = std::make_unique<IdentifierAst>(0uz, std::to_string(i));
            auto field = std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(index));
            auto pstfx = std::make_unique<PostfixExpressionAst>(ast_clone(val), std::move(field));
            auto new_ast = std::make_unique<LetStatementInitializedAst>(nullptr, ast_clone(elem), nullptr, nullptr, std::move(pstfx));
            if (m_from_case_pattern) { new_ast->var->mark_from_case_pattern(); }
            new_ast->stage_7_analyse_semantics(sm, meta);
            m_new_asts.emplace_back(std::move(new_ast));
        }
    }
}


auto spp::asts::LocalVariableDestructureTupleAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the memory state of the elements.
    for (auto &&ast : m_new_asts) {
        ast->stage_8_check_memory(sm, meta);
    }
}


auto spp::asts::LocalVariableDestructureTupleAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Comptime resolve each element.
    for (auto &&x : m_new_asts) {
        x->stage_9_comptime_resolution(sm, meta);
    }
}


auto spp::asts::LocalVariableDestructureTupleAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate the "let" statements for each element.
    for (auto &&ast : m_new_asts) {
        ast->stage_11_code_gen_2(sm, meta, ctx);
    }
    return nullptr;
}
