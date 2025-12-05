module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.unary_expression_operator_async_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.asts.postfix_expression_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;


spp::asts::UnaryExpressionOperatorAsyncAst::UnaryExpressionOperatorAsyncAst(
    decltype(tok_async) &&tok_async) :
    tok_async(std::move(tok_async)) {
}


spp::asts::UnaryExpressionOperatorAsyncAst::~UnaryExpressionOperatorAsyncAst() = default;


auto spp::asts::UnaryExpressionOperatorAsyncAst::pos_start() const
    -> std::size_t {
    return tok_async->pos_start();
}


auto spp::asts::UnaryExpressionOperatorAsyncAst::pos_end() const
    -> std::size_t {
    return tok_async->pos_end();
}


auto spp::asts::UnaryExpressionOperatorAsyncAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<UnaryExpressionOperatorAsyncAst>(
        ast_clone(tok_async));
}


spp::asts::UnaryExpressionOperatorAsyncAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_async);
    SPP_STRING_END;
}


auto spp::asts::UnaryExpressionOperatorAsyncAst::print(
    AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_async);
    SPP_PRINT_END;
}


auto spp::asts::UnaryExpressionOperatorAsyncAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check the right-hand-side is a function call expression.
    if (const auto rhs = meta->unary_expression_rhs->to<PostfixExpressionAst>(); rhs == nullptr or not rhs->op->to<PostfixExpressionOperatorFunctionCallAst>()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppAsyncTargetNotFunctionCallError>()
            .with_args(*tok_async, *this)
            .raises_from(sm->current_scope);
    }
    else {
        // Mark the function call as async.
        rhs->op->to<PostfixExpressionOperatorFunctionCallAst>()->mark_as_async(this);
    }
}


auto spp::asts::UnaryExpressionOperatorAsyncAst::infer_type(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> std::shared_ptr<TypeAst> {
    // Wrap the function call inside a "Future" type.
    auto inner_type = meta->unary_expression_rhs->infer_type(sm, meta);
    auto future_type = generate::common_types::future_type(tok_async->pos_start(), std::move(inner_type));
    future_type->stage_7_analyse_semantics(sm, meta);
    return future_type;
}
