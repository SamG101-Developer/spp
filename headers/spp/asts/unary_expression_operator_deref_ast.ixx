module;
#include <spp/macros.hpp>

export module spp.asts.unary_expression_operator_deref_ast;
import spp.asts.unary_expression_operator_ast;

import std;


SPP_EXP_CLS struct spp::asts::UnaryExpressionOperatorDerefAst final : UnaryExpressionOperatorAst {
    /**
     * The @c * token that indicates a dereference operation. This is used to extract a copyable value from a borrow.
     */
    std::unique_ptr<TokenAst> tok_deref;

    /**
     * Construct the UnaryExpressionOperatorDerefAst with the arguments matching the members.
     * @param tok_deref The @c * token that indicates a dereference operation.
     */
    explicit UnaryExpressionOperatorDerefAst(
        decltype(tok_deref) &&tok_deref);

    ~UnaryExpressionOperatorDerefAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
