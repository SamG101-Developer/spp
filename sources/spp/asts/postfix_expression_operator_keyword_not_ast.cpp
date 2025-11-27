module;
#include <spp/macros.hpp>

module spp.asts.postfix_expression_operator_keyword_not_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.utils.type_utils;
import spp.asts.ast;
import spp.asts.expression_ast;
import spp.asts.token_ast;
import spp.asts.generate.common_types;


spp::asts::PostfixExpressionOperatorKeywordNotAst::PostfixExpressionOperatorKeywordNotAst(
    decltype(tok_dot) &&tok_dot,
    decltype(tok_not) &&tok_not) :
    PostfixExpressionOperatorAst(),
    tok_dot(std::move(tok_dot)),
    tok_not(std::move(tok_not)) {
}


spp::asts::PostfixExpressionOperatorKeywordNotAst::~PostfixExpressionOperatorKeywordNotAst() = default;


auto spp::asts::PostfixExpressionOperatorKeywordNotAst::pos_start() const
    -> std::size_t {
    return tok_dot->pos_start();
}


auto spp::asts::PostfixExpressionOperatorKeywordNotAst::pos_end() const
    -> std::size_t {
    return tok_not->pos_end();
}


auto spp::asts::PostfixExpressionOperatorKeywordNotAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<PostfixExpressionOperatorKeywordNotAst>(
        ast_clone(tok_dot),
        ast_clone(tok_not));
}


spp::asts::PostfixExpressionOperatorKeywordNotAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_APPEND(tok_dot);
    SPP_STRING_APPEND(tok_not);
    SPP_STRING_END;
}


auto spp::asts::PostfixExpressionOperatorKeywordNotAst::print(
    meta::AstPrinter &printer) const
    -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_APPEND(tok_dot);
    SPP_PRINT_APPEND(tok_not);
    SPP_PRINT_END;
}


auto spp::asts::PostfixExpressionOperatorKeywordNotAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Check the left-hand-side is a boolean expression.
    const auto lhs_type = meta->postfix_expression_lhs->infer_type(sm, meta);
    if (not analyse::utils::type_utils::is_type_boolean(*lhs_type, *sm->current_scope)) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppExpressionNotBooleanError>().with_args(
            *meta->postfix_expression_lhs, *lhs_type, "not expression").with_scopes({sm->current_scope}).raise();
    }
}


auto spp::asts::PostfixExpressionOperatorKeywordNotAst::infer_type(
    ScopeManager *,
    meta::CompilerMetaData *)
    -> std::shared_ptr<TypeAst> {
    // The type of a "not" expression is always boolean.
    return generate::common_types::boolean_type(pos_start());
}
