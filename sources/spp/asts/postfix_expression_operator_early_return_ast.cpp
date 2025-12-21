module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_early_return_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.type_utils;
import spp.asts.case_expression_ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_runtime_member_access_ast;
import spp.asts.ret_statement_ast;
import spp.asts.statement_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;


spp::asts::PostfixExpressionOperatorEarlyReturnAst::PostfixExpressionOperatorEarlyReturnAst(
    decltype(tok_qst) &&tok_qst) :
    PostfixExpressionOperatorAst(),
    tok_qst(std::move(tok_qst)) {
}


spp::asts::PostfixExpressionOperatorEarlyReturnAst::~PostfixExpressionOperatorEarlyReturnAst() = default;


auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::pos_start() const
    -> std::size_t {
    return tok_qst->pos_start();
}


auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::pos_end() const
    -> std::size_t {
    return tok_qst->pos_end();
}


auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<PostfixExpressionOperatorEarlyReturnAst>(
        ast_clone(tok_qst));
}


spp::asts::PostfixExpressionOperatorEarlyReturnAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_qst);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_qst);
    SPP_PRINT_END;
}


auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Get the left-hand-side information.
    const auto lhs = meta->postfix_expression_lhs;
    const auto lhs_type = lhs->infer_type(sm, meta);

    // Ensure the left-hand-side superimposes the Try type.
    const auto try_type = analyse::utils::type_utils::get_try_type(*lhs_type, *lhs, *sm);

    // Check the Residual type is compatible with the function's return type.
    const auto residual_type = try_type->type_parts().back()->generic_arg_group->type_at("Residual")->val;
    if (not analyse::utils::type_utils::symbolic_eq(*meta->enclosing_function_ret_type[0], *residual_type, *meta->enclosing_function_scope, *sm->current_scope)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>()
            .with_args(*meta->enclosing_function_ret_type[0], *meta->enclosing_function_ret_type[0], *lhs, *residual_type)
            .raises_from(meta->enclosing_function_scope, sm->current_scope);
    }
}


auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::stage_10_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Convert the unwrapping into a case structure.

    // Create the condition by calling the inspection method on the lhs.
    auto postfix_call = ( {
        auto field_name = std::make_unique<IdentifierAst>(pos_start(), "op_is_residual");
        auto field = std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(field_name));
        auto postfix_field = std::make_unique<PostfixExpressionAst>(ast_clone(meta->postfix_expression_lhs), std::move(field));
        auto call = std::make_unique<PostfixExpressionOperatorFunctionCallAst>(nullptr, nullptr, nullptr);
        std::make_unique<PostfixExpressionAst>(std::move(postfix_field), std::move(call));
    });

    // Create the case arm.
    auto case_branch = ( {
        auto output_field_name = std::make_unique<IdentifierAst>(pos_start(), "op_as_residual");
        auto output_field = std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(output_field_name));
        auto postfix_output_field = std::make_unique<PostfixExpressionAst>(ast_clone(meta->postfix_expression_lhs), std::move(output_field));
        auto call_output = std::make_unique<PostfixExpressionOperatorFunctionCallAst>(nullptr, nullptr, nullptr);
        auto postfix_call_output = std::make_unique<PostfixExpressionAst>(std::move(postfix_output_field), std::move(call_output));
        auto ret_stmt = std::make_unique<RetStatementAst>(nullptr, std::move(postfix_call_output));
        auto inner_scope_members = std::vector<std::unique_ptr<StatementAst>>{};
        inner_scope_members.emplace_back(std::move(ret_stmt));
        std::make_unique<decltype(CaseExpressionBranchAst::body)::element_type>(nullptr, std::move(inner_scope_members), nullptr);
    });

    // Create the case expression.
    const auto case_expr = CaseExpressionAst::new_non_pattern_match(
        nullptr, std::move(postfix_call), std::move(case_branch), {});

    const auto current_scope = sm->current_scope;
    const auto current_scope_iterator = sm->current_iterator();
    case_expr->stage_7_analyse_semantics(sm, meta);
    sm->reset(current_scope, current_scope_iterator);

    return case_expr->stage_10_code_gen_2(sm, meta, ctx);
}


auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Get the left-hand-side information.
    const auto lhs = meta->postfix_expression_lhs;
    const auto lhs_type = lhs->infer_type(sm, meta);

    // Get the Try type's Output generic argument.
    const auto try_type = analyse::utils::type_utils::get_try_type(*lhs_type, *lhs, *sm);
    return try_type->type_parts().back()->generic_arg_group->type_at("Output")->val;
}
