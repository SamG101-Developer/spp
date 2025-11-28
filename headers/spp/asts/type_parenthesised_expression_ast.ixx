module;
#include <spp/macros.hpp>

export module spp.asts.type_parenthesised_expression_ast;
import spp.asts._fwd;
import spp.asts.ast;
import spp.asts.mixins.temp_type_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct TypeParenthesisedExpressionAst;
}


SPP_EXP_CLS struct spp::asts::TypeParenthesisedExpressionAst final : virtual Ast, mixins::TempTypeAst {
    /**
     * The left parenthesis token that represents the start of the parenthesised expression.
     */
    std::unique_ptr<TokenAst> tok_l;

    /**
     * The type expression that is enclosed within the parentheses.
     */
    std::shared_ptr<TypeAst> expr;

    /**
     * The right parenthesis token that represents the end of the parenthesised expression.
     */
    std::unique_ptr<TokenAst> tok_r;

    /**
     * Construct the TypeParenthesisedExpressionAst with the arguments matching the members.
     * @param tok_l The left parenthesis token.
     * @param expr The type expression within the parentheses.
     * @param tok_r The right parenthesis token.
     */
    TypeParenthesisedExpressionAst(
        decltype(tok_l) &&tok_l,
        decltype(expr) &&expr,
        decltype(tok_r) &&tok_r);

    ~TypeParenthesisedExpressionAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto convert() -> std::unique_ptr<TypeAst> override;
};
