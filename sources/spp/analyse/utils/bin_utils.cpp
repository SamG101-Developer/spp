module spp.analyse.utils.bin_utils;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.asts.ast;
import spp.asts.binary_expression_ast;
import spp.asts.case_expression_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_ast;
import spp.asts.convention_ref_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.function_call_argument_positional_ast;
import spp.asts.fold_expression_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.is_expression_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
import spp.asts.pattern_guard_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import spp.utils.uid;
import genex;


auto spp::analyse::utils::bin_utils::fix_associativity(
    asts::BinaryExpressionAst &bin_expr,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> std::unique_ptr<asts::BinaryExpressionAst> {
    // If the right-hand-side isn't a binary expression, then no handling is required; return it.
    if (bin_expr.rhs->to<asts::BinaryExpressionAst>() == nullptr) {
        return std::make_unique<asts::BinaryExpressionAst>(
            std::move(bin_expr.lhs),
            std::move(bin_expr.tok_op),
            std::move(bin_expr.rhs));
    }

    // If the ast precedence > the right-hand-side binary expression's operator's precedence, re-arrange the AST.
    auto bin_rhs = std::unique_ptr<asts::BinaryExpressionAst>(bin_expr.rhs.release()->to<asts::BinaryExpressionAst>());
    if (BIN_OP_PRECEDENCE.at(bin_expr.tok_op->token_type) >= BIN_OP_PRECEDENCE.at(bin_rhs->tok_op->token_type)) {
        bin_expr.rhs = std::move(bin_rhs->rhs);
        bin_rhs->rhs = std::move(bin_rhs->lhs);
        bin_rhs->lhs = std::move(bin_expr.lhs);

        auto temp = std::move(bin_rhs->tok_op);
        bin_rhs->tok_op = std::move(bin_expr.tok_op);
        bin_expr.tok_op = std::move(temp);
        bin_expr.lhs = std::move(bin_rhs);

        return fix_associativity(bin_expr, sm, meta);
    }

    // Otherwise, the AST is in the correct order; return it.
    return std::make_unique<asts::BinaryExpressionAst>(
        std::move(bin_expr.lhs),
        std::move(bin_expr.tok_op),
        std::move(bin_rhs));
}


auto spp::analyse::utils::bin_utils::combine_comp_ops(
    asts::BinaryExpressionAst &bin_expr,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> std::unique_ptr<asts::BinaryExpressionAst> {
    // Check the left-hand-side is a binary expression with a comparison operator.
    const auto bin_lhs = bin_expr.lhs->to<asts::BinaryExpressionAst>();
    if (
        bin_lhs == nullptr or
        not genex::contains(BIN_COMPARISON_OPS, bin_expr.tok_op->token_type) or
        not genex::contains(BIN_COMPARISON_OPS, bin_lhs->tok_op->token_type)) {
        return std::make_unique<asts::BinaryExpressionAst>(
            std::move(bin_expr.lhs),
            std::move(bin_expr.tok_op),
            std::move(bin_expr.rhs));
    }

    // Non-symbolic value being re-used -> put it into a variable first.
    if (sm->current_scope->get_var_symbol_outermost(*bin_lhs->rhs).first == nullptr) {
        const auto uid = spp::utils::generate_uid(bin_lhs->rhs.get());
        auto temp_var_name = std::make_shared<asts::IdentifierAst>(bin_lhs->rhs->pos_start(), uid);
        auto temp_var_ast = std::make_unique<asts::LocalVariableSingleIdentifierAst>(nullptr, temp_var_name, nullptr);
        const auto temp_let = std::make_unique<asts::LetStatementInitializedAst>(nullptr, std::move(temp_var_ast), nullptr, nullptr, std::move(bin_lhs->rhs));
        temp_let->stage_7_analyse_semantics(sm, meta);
        bin_lhs->rhs = asts::ast_clone(temp_var_name);
    }

    // Otherwise, re-arrange the ASTs, with an "and" combinator binary expression.
    auto lhs = asts::ast_clone(bin_lhs->rhs);
    auto rhs = std::move(bin_expr.rhs);
    auto op_pos = bin_expr.tok_op->pos_start();
    bin_expr.rhs = std::make_unique<asts::BinaryExpressionAst>(std::move(lhs), std::move(bin_expr.tok_op), std::move(rhs));
    bin_expr.tok_op = std::make_unique<asts::TokenAst>(op_pos, lex::SppTokenType::KW_AND, "and");

    return combine_comp_ops(bin_expr, sm, meta);
}


auto spp::analyse::utils::bin_utils::convert_bin_expr_to_function_call(
    asts::BinaryExpressionAst &bin_expr,
    scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta)
    -> std::unique_ptr<asts::PostfixExpressionAst> {
    // Call other utility methods that may modify the binary expression AST.
    auto new_bin_expr = fix_associativity(bin_expr, sm, meta);
    new_bin_expr = combine_comp_ops(*new_bin_expr, sm, meta);

    // Get the method names based on the operator token.
    auto method_name = BIN_METHODS.at(new_bin_expr->tok_op->token_type);
    auto method_name_wrapped = std::make_unique<asts::IdentifierAst>(new_bin_expr->tok_op->pos_start(), std::move(method_name));

    // Construct the function call AST.
    auto field = std::make_unique<asts::PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(method_name_wrapped));
    auto field_access = std::make_unique<asts::PostfixExpressionAst>(std::move(new_bin_expr->lhs), std::move(field));
    auto fn_call = std::make_unique<asts::PostfixExpressionOperatorFunctionCallAst>(nullptr, nullptr, nullptr);

    // Set the arguments for the function call, and return the AST.
    auto conv = genex::contains(BIN_COMPARISON_OPS, new_bin_expr->tok_op->token_type) ? std::make_unique<asts::ConventionRefAst>(nullptr) : nullptr;
    auto arg = std::make_unique<asts::FunctionCallArgumentPositionalAst>(std::move(conv), nullptr, std::move(new_bin_expr->rhs));
    fn_call->arg_group->args.emplace_back(std::move(arg));
    auto new_ast = std::make_unique<asts::PostfixExpressionAst>(std::move(field_access), std::move(fn_call));
    return new_ast;
}


auto spp::analyse::utils::bin_utils::convert_is_expr_to_function_call(
    asts::IsExpressionAst &is_expr,
    scopes::ScopeManager *,
    asts::meta::CompilerMetaData *)
    -> std::unique_ptr<asts::CaseExpressionAst> {
    // Construct the expression-pattern based on the right-hand-side of the "x is Type".
    auto pattern = std::move(is_expr.rhs);
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
