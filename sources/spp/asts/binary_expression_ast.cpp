#include <tuple>

#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/bin_utils.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/binary_expression_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/postfix_expression_operator_ast.hpp>
#include <spp/asts/postfix_expression_operator_runtime_member_access_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>
#include <spp/lex/tokens.hpp>
#include <spp/parse/parser_base.hpp>

#include <genex/algorithms/contains.hpp>
#include <genex/views/drop.hpp>
#include <genex/views/move.hpp>
#include <genex/views/take.hpp>


spp::asts::BinaryExpressionAst::BinaryExpressionAst(
    decltype(lhs) &&lhs,
    decltype(tok_op) &&tok_op,
    decltype(rhs) &&rhs) :
    m_mapped_func(nullptr),
    lhs(std::move(lhs)),
    tok_op(std::move(tok_op)),
    rhs(std::move(rhs)) {
}


spp::asts::BinaryExpressionAst::~BinaryExpressionAst() = default;


auto spp::asts::BinaryExpressionAst::pos_start() const -> std::size_t {
    return lhs->pos_start();
}


auto spp::asts::BinaryExpressionAst::pos_end() const -> std::size_t {
    return rhs->pos_end();
}


auto spp::asts::BinaryExpressionAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<BinaryExpressionAst>(
        ast_clone(lhs),
        ast_clone(tok_op),
        ast_clone(rhs));
}


spp::asts::BinaryExpressionAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(lhs);
    SPP_STRING_APPEND(tok_op);
    SPP_STRING_APPEND(rhs);
    SPP_STRING_END;
}


auto spp::asts::BinaryExpressionAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(lhs);
    SPP_PRINT_APPEND(tok_op);
    SPP_PRINT_APPEND(rhs);
    SPP_PRINT_END;
}


auto spp::asts::BinaryExpressionAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Ensure TypeAst's aren't used for expression for binary operands.
    ENFORCE_EXPRESSION_SUBTYPE_ALLOW_TOKEN(lhs.get());
    ENFORCE_EXPRESSION_SUBTYPE_ALLOW_TOKEN(rhs.get());

    // Check compound assignment (for example "+=") has a symbolic lhs target.
    if (genex::algorithms::contains(analyse::utils::bin_utils::BIN_COMPOUND_ASSIGNMENT_OPS, tok_op->token_type)) {
        if (not sm->current_scope->get_var_symbol_outermost(*lhs).first) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppCompoundAssignmentTargetError>().with_args(
                *lhs).with_scopes({sm->current_scope}).raise();
        }
    }

    // Handle lhs-folding.
    if (ast_cast<TokenAst>(lhs.get())) {
        // Check the rhs is a tuple.
        const auto rhs_tuple_type = rhs->infer_type(sm, meta);
        if (not analyse::utils::type_utils::is_type_tuple(*rhs_tuple_type, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppMemberAccessNonIndexableError>().with_args(
                *rhs, *rhs_tuple_type, *lhs).with_scopes({sm->current_scope}).raise();
        }

        // Get the parts of the tuple.
        const auto rhs_num_elems = rhs_tuple_type->type_parts()[0]->generic_arg_group->args.size();
        auto new_asts = std::vector<std::unique_ptr<PostfixExpressionAst>>();
        for (auto i = 0u; i < rhs_num_elems; ++i) {
            auto field = std::make_unique<IdentifierAst>(rhs->pos_start(), std::to_string(i));
            auto new_ast = std::make_unique<PostfixExpressionAst>(
                ast_clone(lhs),
                std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field)));
            new_ast->stage_7_analyse_semantics(sm, meta);
            new_asts.emplace_back(std::move(new_ast));
        }

        // Convert "t = (0, 1, 2, 3)", ".. + t" into "(((t.0 + t.1) + t.2) + t.3)".
        lhs = std::move(new_asts[0]);
        rhs = std::move(new_asts[1]);
        for (auto &&new_ast : new_asts | genex::views::move | genex::views::drop(2) | genex::views::to<std::vector>()) {
            lhs = std::make_unique<BinaryExpressionAst>(std::move(lhs), ast_clone(tok_op), std::move(rhs));
            rhs = std::move(new_ast);
        }
        m_mapped_func = analyse::utils::bin_utils::convert_bin_expr_to_function_call(*this);
        m_mapped_func->stage_7_analyse_semantics(sm, meta);
    }

    // Handle rhs-folding.
    if (ast_cast<TokenAst>(rhs.get())) {
        // Check the lhs is a tuple.
        const auto lhs_tuple_type = lhs->infer_type(sm, meta);
        if (not analyse::utils::type_utils::is_type_tuple(*lhs_tuple_type, *sm->current_scope)) {
            analyse::errors::SemanticErrorBuilder<analyse::errors::SppMemberAccessNonIndexableError>().with_args(
                *lhs, *lhs_tuple_type, *rhs).with_scopes({sm->current_scope}).raise();
        }

        // Get the parts of the tuple.
        const auto lhs_num_elems = lhs_tuple_type->type_parts()[0]->generic_arg_group->args.size();
        auto new_asts = std::vector<std::unique_ptr<PostfixExpressionAst>>();
        for (auto i = 0u; i < lhs_num_elems; ++i) {
            auto field = std::make_unique<IdentifierAst>(rhs->pos_start(), std::to_string(i));
            auto new_ast = std::make_unique<PostfixExpressionAst>(
                ast_clone(rhs),
                std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field)));
            new_ast->stage_7_analyse_semantics(sm, meta);
            new_asts.emplace_back(std::move(new_ast));
        }

        // Convert "t = (0, 1, 2, 3)", "t + .." into "(t.0 + (t.1 + (t.2 + t.3)))".
        rhs = std::move(new_asts[new_asts.size() - 2]);
        lhs = std::move(new_asts[new_asts.size() - 1]);
        for (auto &&new_ast : new_asts | genex::views::move | genex::views::take(new_asts.size() - 1) | genex::views::to<std::vector>()) {
            lhs = std::move(new_ast);
            rhs = std::make_unique<BinaryExpressionAst>(std::move(lhs), ast_clone(tok_op), std::move(rhs));
        }
        m_mapped_func = analyse::utils::bin_utils::convert_bin_expr_to_function_call(*this);
        m_mapped_func->stage_7_analyse_semantics(sm, meta);
    }

    else {
        // Standard non-folding binary expression.
        m_mapped_func = analyse::utils::bin_utils::convert_bin_expr_to_function_call(*this);
        m_mapped_func->stage_7_analyse_semantics(sm, meta);
    }
}


auto spp::asts::BinaryExpressionAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward the memory checking to the mapped function.
    m_mapped_func->stage_8_check_memory(sm, meta);
}


auto spp::asts::BinaryExpressionAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Infer the type from the function mapping of the binary expression.
    if (m_mapped_func == nullptr) {
        stage_7_analyse_semantics(sm, meta);
    }
    return m_mapped_func->infer_type(sm, meta);
}
