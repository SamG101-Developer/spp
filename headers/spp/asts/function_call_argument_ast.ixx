module;
#include <spp/macros.hpp>

export module spp.asts.function_call_argument_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct FunctionCallArgumentAst;
    SPP_EXP_CLS struct PostfixExpressionOperatorFunctionCallAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The FunctionCallArgumentAst is the base class representing an argument in a function call. It is inherited into the
 * "positional" and "keyword" variants.
 */
SPP_EXP_CLS struct spp::asts::FunctionCallArgumentAst : Ast {
    std::unique_ptr<ConventionAst> conv;
    std::unique_ptr<ExpressionAst> val;

    explicit FunctionCallArgumentAst(
        decltype(conv) &&conv,
        decltype(val) &&val);
    ~FunctionCallArgumentAst() override;
    auto to_rust() const -> std::string override;
};
