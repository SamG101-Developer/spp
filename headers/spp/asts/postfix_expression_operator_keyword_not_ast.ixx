module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_keyword_not_ast;
import spp.asts.postfix_expression_operator_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct PostfixExpressionOperatorKeywordNotAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorKeywordNotAst final : PostfixExpressionOperatorAst {
    /**
     * The @c . token that indicates a member access operation.
     */
    Unique<TokenAst> TokDot;

    /**
     * The @c not token that indicates a keyword not operation.
     */
    Unique<TokenAst> TokNot;

    /**
     * Construct the PostfixExpressionOperatorKeywordNotAst with the arguments matching the members.
     * @param tok_dot The @c . token that indicates a member access operation.
     * @param tok_not The @c not token that indicates a keyword not operation.
     */
    PostfixExpressionOperatorKeywordNotAst(
        decltype(TokDot) &&tok_dot,
        decltype(TokNot) &&tok_not);

    ~PostfixExpressionOperatorKeywordNotAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto Stage9_CompTimeResolve(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto InferType(ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> override;
};
