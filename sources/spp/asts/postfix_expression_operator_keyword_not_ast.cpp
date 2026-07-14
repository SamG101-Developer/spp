module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.postfix_expression_operator_keyword_not_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.utils.type_utils;
import spp.asts.boolean_literal_ast;
import spp.asts.expression_ast;
import spp.asts.token_ast;
import spp.asts.type_ast;
import spp.asts.generate.common_types;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;

SPP_MOD_BEGIN
spp::asts::PostfixExpressionOperatorKeywordNotAst::PostfixExpressionOperatorKeywordNotAst(
    decltype(TokDot) &&tok_dot,
    decltype(TokNot) &&tok_not) :
    TokDot(std::move(tok_dot)),
    TokNot(std::move(tok_not)) {
}

spp::asts::PostfixExpressionOperatorKeywordNotAst::~PostfixExpressionOperatorKeywordNotAst() = default;

auto spp::asts::PostfixExpressionOperatorKeywordNotAst::PosStart() const
    -> std::size_t {
    // Use the "." token.
    return TokDot->PosStart();
}

auto spp::asts::PostfixExpressionOperatorKeywordNotAst::PosEnd() const
    -> std::size_t {
    // Use the "not" token.
    return TokNot->PosEnd();
}

auto spp::asts::PostfixExpressionOperatorKeywordNotAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<PostfixExpressionOperatorKeywordNotAst>(
        AstClone(TokDot), AstClone(TokNot));
}

auto spp::asts::PostfixExpressionOperatorKeywordNotAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_APPEND(TokDot);
    SPP_STRING_APPEND(TokNot);
    SPP_STRING_END;
}

auto spp::asts::PostfixExpressionOperatorKeywordNotAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    //
    using analyse::errors::SppExpressionNotBooleanError;
    using analyse::utils::type_utils::IsTypeBool;

    // Check the left-hand-side is a boolean expression.
    const auto lhs_type = meta->PostfixExpressionLhs->InferType(sm, meta);
    RaiseIf<SppExpressionNotBooleanError>(
        not IsTypeBool(*lhs_type, *sm->CurrentScope),
        {sm->CurrentScope}, ERR_ARGS(*meta->PostfixExpressionLhs, *lhs_type, "not expression"));
}

auto spp::asts::PostfixExpressionOperatorKeywordNotAst::Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // The "lhs" will be boolean based on previous analysis.
    meta->PostfixExpressionLhs->Stage9_CompTimeResolve(sm, meta);
    const auto cmp_lhs_bool = meta->CmpResult->To<BooleanLiteralAst>();

    // Extract the value inside the boolean and invert it.
    const auto p = PosStart();
    meta->CmpResult = cmp_lhs_bool->IsTrue() ? BooleanLiteralAst::False(p) : BooleanLiteralAst::True(p);
}

auto spp::asts::PostfixExpressionOperatorKeywordNotAst::InferType(
    analyse::scopes::ScopeManager *,
    meta::CompilerMetaData *)
    -> TypeAst* {
    // Try from the cache first.
    using generate::common_types::BooleanType;
    USE_CACHED_TYPE_INFERENCE;

    // The type of a "not" expression is always boolean.
    auto inferred = BooleanType(PosStart());
    CACHE_TYPE_INFERENCE_AND_RETURN(inferred);
}

SPP_MOD_END
