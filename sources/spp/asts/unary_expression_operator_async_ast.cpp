#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/unary_expression_operator_async_ast.hpp>
#include <spp/asts/expression_ast.hpp>
#include <spp/asts/postfix_expression_ast.hpp>
#include <spp/asts/postfix_expression_operator_function_call_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>
#include <spp/asts/generate/common_types.hpp>


spp::asts::UnaryExpressionOperatorAsyncAst::UnaryExpressionOperatorAsyncAst(
    decltype(tok_async) &&tok_async) :
    tok_async(std::move(tok_async)) {
}


spp::asts::UnaryExpressionOperatorAsyncAst::~UnaryExpressionOperatorAsyncAst() = default;


auto spp::asts::UnaryExpressionOperatorAsyncAst::pos_start() const -> std::size_t {
    return tok_async->pos_start();
}


auto spp::asts::UnaryExpressionOperatorAsyncAst::pos_end() const -> std::size_t {
    return tok_async->pos_end();
}


auto spp::asts::UnaryExpressionOperatorAsyncAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<UnaryExpressionOperatorAsyncAst>(
        ast_clone(tok_async));
}


spp::asts::UnaryExpressionOperatorAsyncAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_async);
    SPP_STRING_END;
}


auto spp::asts::UnaryExpressionOperatorAsyncAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_async);
    SPP_PRINT_END;
}


auto spp::asts::UnaryExpressionOperatorAsyncAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check the right-hand-side is a function call expression.
    if (const auto rhs = ast_cast<PostfixExpressionAst>(meta->unary_expression_rhs); rhs == nullptr or not ast_cast<PostfixExpressionOperatorFunctionCallAst>(rhs->op.get())) {
        analyse::errors::SppAsyncTargetNotFunctionCallError(*tok_async, *rhs).scopes({sm->current_scope}).raise();
    }
    else {
        // Mark the function call as async.
        rhs->op->m_is_async = true;
    }
}


auto spp::asts::UnaryExpressionOperatorAsyncAst::infer_type(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Wrap the function call inside a "Future" type.
    auto inner_type = meta->unary_expression_rhs->infer_type(sm, meta);
    auto future_type = generate::common_types::future_type(tok_async->pos_start(), std::move(inner_type));
    future_type->stage_7_analyse_semantics(sm, meta);
    return future_type;
}
