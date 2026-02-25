module;
#include <spp/macros.hpp>

export module spp.asts.unary_expression_operator_async_ast;
import spp.asts.unary_expression_operator_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct UnaryExpressionOperatorAsyncAst;
}


SPP_EXP_CLS struct spp::asts::UnaryExpressionOperatorAsyncAst final : UnaryExpressionOperatorAst {
    std::unique_ptr<TokenAst> tok_async;

    explicit UnaryExpressionOperatorAsyncAst(
        decltype(tok_async) &&tok_async);
    ~UnaryExpressionOperatorAsyncAst() override;
    auto to_rust() const -> std::string override;
};
