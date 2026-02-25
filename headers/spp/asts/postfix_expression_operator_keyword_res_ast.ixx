module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_keyword_res_ast;
import spp.asts.postfix_expression_operator_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct PostfixExpressionOperatorKeywordResAst;
    SPP_EXP_CLS struct PostfixExpressionAst;
    SPP_EXP_CLS struct FunctionCallArgumentGroupAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorKeywordResAst final : PostfixExpressionOperatorAst {
    std::unique_ptr<FunctionCallArgumentGroupAst> arg_group;

    explicit PostfixExpressionOperatorKeywordResAst(
        decltype(arg_group) &&arg_group);
    ~PostfixExpressionOperatorKeywordResAst() override;
    auto to_rust() const -> std::string override;
};
