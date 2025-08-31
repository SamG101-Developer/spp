#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/analyse/utils/type_utils.hpp>
#include <spp/asts/generic_argument_type_ast.hpp>
#include <spp/asts/generic_argument_group_ast.hpp>
#include <spp/asts/postfix_expression_operator_early_return_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/type_identifier_ast.hpp>


spp::asts::PostfixExpressionOperatorEarlyReturnAst::PostfixExpressionOperatorEarlyReturnAst(
    decltype(tok_qst) &&tok_qst) :
    PostfixExpressionOperatorAst(),
    tok_qst(std::move(tok_qst)) {
}


spp::asts::PostfixExpressionOperatorEarlyReturnAst::~PostfixExpressionOperatorEarlyReturnAst() = default;


auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::pos_start() const -> std::size_t {
    return tok_qst->pos_start();
}


auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::pos_end() const -> std::size_t {
    return tok_qst->pos_end();
}


auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<PostfixExpressionOperatorEarlyReturnAst>(
        ast_clone(tok_qst));
}


spp::asts::PostfixExpressionOperatorEarlyReturnAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_qst);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_qst);
    SPP_PRINT_END;
}


auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Get the left-hand-side information.
    const auto lhs = meta->postfix_expression_lhs;
    const auto lhs_type = lhs->infer_type(sm, meta);

    // Ensure the left-hand-side superimposes the Try type.
    const auto try_type = analyse::utils::type_utils::get_try_type(*lhs_type, *sm);
    if (try_type == nullptr) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppEarlyReturnRequiresTryTypeError>().with_args(
            *this, *lhs, *lhs_type).with_scopes({sm->current_scope}).raise();
    }

    // Check the Residual type is compatible with the function's return type.
    const auto residual_type = try_type->type_parts().back()->generic_arg_group->type_at("Residual")->val;
    if (not analyse::utils::type_utils::symbolic_eq(*meta->enclosing_function_ret_type[0], *residual_type, *meta->enclosing_function_scope, *sm->current_scope)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppTypeMismatchError>().with_args(
            *meta->enclosing_function_ret_type[0], *meta->enclosing_function_ret_type[0], *lhs, *residual_type).with_scopes({meta->enclosing_function_scope, sm->current_scope}).raise();
    }
}


auto spp::asts::PostfixExpressionOperatorEarlyReturnAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Get the left-hand-side information.
    const auto lhs = meta->postfix_expression_lhs;
    const auto lhs_type = lhs->infer_type(sm, meta);

    // Get the Try type's Output generic argument.
    const auto try_type = analyse::utils::type_utils::get_try_type(*lhs_type, *sm);
    return try_type->type_parts().back()->generic_arg_group->type_at("Output")->val;
}
