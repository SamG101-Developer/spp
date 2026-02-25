module;
#include <spp/macros.hpp>

export module spp.asts.postfix_expression_operator_function_call_ast;
import spp.asts.postfix_expression_operator_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct FunctionCallArgumentGroupAst;
    SPP_EXP_CLS struct FoldExpressionAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorFunctionCallAst;
}

namespace spp::analyse::scopes {
    SPP_EXP_CLS class Scope;
}


SPP_EXP_CLS struct spp::asts::PostfixExpressionOperatorFunctionCallAst final : PostfixExpressionOperatorAst {
    std::unique_ptr<GenericArgumentGroupAst> generic_arg_group;
    std::unique_ptr<FunctionCallArgumentGroupAst> arg_group;
    std::unique_ptr<FoldExpressionAst> fold;

    explicit PostfixExpressionOperatorFunctionCallAst(
        decltype(generic_arg_group) &&generic_arg_group,
        decltype(arg_group) &&arg_group,
        decltype(fold) &&fold);
    ~PostfixExpressionOperatorFunctionCallAst() override;
    auto to_rust() const -> std::string override;
};
