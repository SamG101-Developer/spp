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
import spp.asts.case_pattern_variant_ast;
import spp.asts.case_pattern_variant_destructure_object_ast;
import spp.asts.case_pattern_variant_destructure_skip_multiple_arguments_ast;
import spp.asts.expression_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.is_expression_ast;
import spp.asts.fold_expression_ast;
import spp.asts.function_call_argument_group_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_argument_type_ast;
import spp.asts.let_statement_initialized_ast;
import spp.asts.local_variable_single_identifier_ast;
import spp.asts.local_variable_single_identifier_alias_ast;
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
import spp.codegen.llvm_materialize;
import spp.utils.uid;


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


auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Extract the "try" information for the type.
    auto lhs = meta->postfix_expression_lhs;
    const auto lhs_type = lhs->infer_type(sm, meta);
    const auto try_type = analyse::utils::type_utils::get_try_type(*lhs_type, *lhs, *sm);

    // Temp holder for non-symbolic condition.
    if (sm->current_scope->get_var_symbol_outermost(*lhs).first == nullptr) {
        lhs = codegen::llvm_materialize(*lhs, sm, meta, ctx);
    }

    // Create the condition by attempting an "is" against the residual.
    auto type_check = ( {
        auto is_expr_lhs = ast_clone(lhs);
        auto is_expr_rhs = try_type->type_parts().back()->generic_arg_group->type_at("Residual")->val;

        auto skip_all = std::make_unique<CasePatternVariantDestructureSkipMultipleArgumentsAst>(nullptr, nullptr);
        auto destructures = std::vector<std::unique_ptr<CasePatternVariantAst>>{};
        destructures.emplace_back(std::move(skip_all));

        auto is_expr_rhs_wrap = std::make_unique<CasePatternVariantDestructureObjectAst>(std::move(is_expr_rhs), nullptr, std::move(destructures), nullptr);
        auto is_expr = std::make_unique<IsExpressionAst>(std::move(is_expr_lhs), nullptr, std::move(is_expr_rhs_wrap));
        std::move(is_expr);
    });

    auto ret = ( {
        auto ret_expr = ast_clone(lhs);
        std::make_unique<RetStatementAst>(nullptr, std::move(ret_expr));
    });

    // Convert the "is" expression into a case condition with destructure.
    const auto current_scope = sm->current_scope;
    const auto current_scope_iterator = sm->current_iterator();

    type_check->stage_7_analyse_semantics(sm, meta);
    const auto check = type_check->mapped_func();
    check->branches[0]->body->members.emplace_back(std::move(ret));
    sm->reset(current_scope, current_scope_iterator);

    meta->save();
    meta->assignment_target = nullptr;
    meta->assignment_target_type = nullptr;
    check->stage_11_code_gen_2(sm, meta, ctx);
    meta->restore();

    // Next, in case the try succeeded, we need to extract the Output value.
    auto output_extract = ( {
        auto output_field_name = std::make_unique<IdentifierAst>(pos_start(), "op_as_value");
        auto output_field = std::make_unique<PostfixExpressionOperatorRuntimeMemberAccessAst>(nullptr, std::move(output_field_name));
        auto postfix_output_field = std::make_unique<PostfixExpressionAst>(ast_clone(lhs), std::move(output_field));
        auto call_output = std::make_unique<PostfixExpressionOperatorFunctionCallAst>(nullptr, nullptr, nullptr);
        auto postfix_call_output = std::make_unique<PostfixExpressionAst>(std::move(postfix_output_field), std::move(call_output));
        std::move(postfix_call_output);
    });

    output_extract->stage_7_analyse_semantics(sm, meta); // Never going to create a scope.
    return output_extract->stage_11_code_gen_2(sm, meta, ctx);
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
    return try_type->type_parts().back()->generic_arg_group->type_at("Value")->val;
}
