#include <spp/analyse/utils/bin_utils.hpp>
#include <spp/asts/binary_expression_ast.hpp>
#include <spp/asts/case_expression_ast.hpp>
#include <spp/asts/case_expression_branch_ast.hpp>
#include <spp/asts/case_pattern_variant_expression_ast.hpp>
#include <spp/asts/convention_mov_ast.hpp>
#include <spp/asts/convention_ref_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/is_expression_ast.hpp>
#include <spp/asts/fold_expression_ast.hpp>
#include <spp/asts/function_call_argument_group_ast.hpp>
#include <spp/asts/function_call_argument_positional_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/inner_scope_expression_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/pattern_guard_ast.hpp>
#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/postfix_expression_operator_function_call_ast.hpp>
#include <spp/asts/postfix_expression_operator_runtime_member_access_ast.hpp>
#include <spp/asts/token_ast.hpp>

#include <genex/algorithms/contains.hpp>


auto spp::analyse::utils::bin_utils::fix_associativity(
    asts::BinaryExpressionAst &bin_expr)
    -> std::unique_ptr<asts::BinaryExpressionAst> {
    // If the right-hand-side isn't a binary expression, then no handling is required; return it.
    const auto bin_rhs = asts::ast_cast<asts::BinaryExpressionAst>(bin_expr.rhs.get());
    if (bin_rhs == nullptr) {
        return std::make_unique<asts::BinaryExpressionAst>(std::move(bin_expr.lhs), std::move(bin_expr.tok_op), std::move(bin_expr.rhs));
    }

    // If the ast precedence > the right-hand-side binary expression's operator's precedence, re-arrange the AST.
    if (BIN_OP_PRECEDENCE.at(bin_expr.tok_op->token_type) >= BIN_OP_PRECEDENCE.at(bin_rhs->tok_op->token_type)) {
        auto rhs = std::move(bin_expr.rhs);
        bin_expr.rhs = std::move(bin_rhs->rhs);
        bin_rhs->rhs = std::move(bin_rhs->lhs);
        bin_rhs->lhs = std::move(bin_expr.lhs);

        auto temp = std::move(bin_rhs->tok_op);
        bin_rhs->tok_op = std::move(bin_expr.tok_op);
        bin_expr.tok_op = std::move(temp);

        return fix_associativity(bin_expr);
    }

    // Otherwise, the AST is in the correct order; return it.
    return std::make_unique<asts::BinaryExpressionAst>(
        std::move(bin_expr.lhs),
        std::move(bin_expr.tok_op),
        std::move(bin_expr.rhs));
}


auto spp::analyse::utils::bin_utils::combine_comp_ops(
    asts::BinaryExpressionAst &bin_expr)
    -> std::unique_ptr<asts::BinaryExpressionAst> {
    // Check the left-hand-side is a binary expression with a comparison operator.
    const auto bin_lhs = asts::ast_cast<asts::BinaryExpressionAst>(bin_expr.lhs.get());
    if (
        bin_lhs == nullptr or
        not genex::algorithms::contains(BIN_COMPARISON_OPS, bin_expr.tok_op->token_type) or
        not genex::algorithms::contains(BIN_COMPARISON_OPS, bin_lhs->tok_op->token_type)) {
        return std::make_unique<asts::BinaryExpressionAst>(
            std::move(bin_expr.lhs),
            std::move(bin_expr.tok_op),
            std::move(bin_expr.rhs));
    }

    // Otherwise, re-arrange the ASTs, with an "and" combinator binary expression.
    auto lhs = std::move(bin_lhs->rhs);
    auto rhs = std::move(bin_expr.rhs);
    bin_expr.rhs = std::make_unique<asts::BinaryExpressionAst>(std::move(lhs), std::move(bin_expr.tok_op), std::move(rhs));
    bin_expr.tok_op = std::make_unique<asts::TokenAst>(bin_expr.tok_op->pos_start(), lex::SppTokenType::KW_AND, "and");
    combine_comp_ops(bin_expr);
    return std::make_unique<asts::BinaryExpressionAst>(
        std::move(bin_lhs->lhs),
        std::move(bin_expr.tok_op),
        std::move(bin_expr.rhs));
}


auto spp::analyse::utils::bin_utils::convert_bin_expr_to_function_call(
    asts::BinaryExpressionAst &bin_expr)
    -> std::unique_ptr<asts::PostfixExpressionAst> {
    // Get the method names based on the operator token.
    auto method_name = BIN_METHODS.at(bin_expr.tok_op->token_type);
    auto method_name_wrapped = std::make_unique<asts::IdentifierAst>(bin_expr.tok_op->pos_start(), std::move(method_name));

    // Construct the function call AST.
    auto field = std::make_unique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(method_name_wrapped));
    auto field_access = std::make_unique<asts::PostfixExpressionAst>(std::move(bin_expr.lhs), std::move(field));
    auto fn_call = std::make_unique<asts::PostfixExpressionOperatorFunctionCallAst>(nullptr, nullptr, nullptr);
    auto new_ast = std::make_unique<asts::PostfixExpressionAst>(std::move(field_access), std::move(fn_call));

    // Apply the correct "self" convention based on the operator.
    auto convention = asts::ast_cast<asts::ConventionAst>(std::make_unique<asts::ConventionMovAst>());
    if (genex::algorithms::contains(BIN_COMPARISON_OPS, bin_expr.tok_op->token_type)) {
        convention = std::make_unique<asts::ConventionRefAst>(nullptr);
    }

    // Set the arguments for the function call, and return the AST.
    auto arg = std::make_unique<asts::FunctionCallArgumentPositionalAst>(std::move(convention), nullptr, std::move(bin_expr.rhs));
    fn_call->arg_group->args.emplace_back(std::move(arg));
    return new_ast;
}


auto spp::analyse::utils::bin_utils::convert_is_expr_to_function_call(
    asts::IsExpressionAst &is_expr)
    -> std::unique_ptr<asts::CaseExpressionAst> {
    // Construct the expression-pattern based on the right-hand-side of the "x is Type".
    auto pattern = asts::ast_cast<asts::CasePatternVariantExpressionAst>(std::move(is_expr.rhs));
    auto patterns = std::vector<std::unique_ptr<asts::CasePatternVariantAst>>();
    patterns.emplace_back(std::move(pattern));

    // Construct the case expression branch that contains the pattern.
    auto branch = std::make_unique<asts::CaseExpressionBranchAst>(std::move(is_expr.tok_op), std::move(patterns), nullptr, nullptr);
    auto branches = std::vector<std::unique_ptr<asts::CaseExpressionBranchAst>>();
    branches.emplace_back(std::move(branch));

    // Construct and return the case expression AST.
    auto case_expr = std::make_unique<asts::CaseExpressionAst>(nullptr, std::move(is_expr.lhs), nullptr, std::move(branches));
    return case_expr;
}
