module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_group_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct FunctionParameterGroupAst;
    SPP_EXP_CLS struct FunctionParameterAst;
    SPP_EXP_CLS struct TokenAst;
}


/**
 * A FunctionParameterGroupAst is used to represent a group of function parameters in a function prototype.
 */
SPP_EXP_CLS struct spp::asts::FunctionParameterGroupAst final : Ast {
    std::unique_ptr<TokenAst> tok_l;
    std::vector<std::unique_ptr<FunctionParameterAst>> params;
    std::unique_ptr<TokenAst> tok_r;

    FunctionParameterGroupAst(
        decltype(tok_l) &&tok_l,
        decltype(params) &&params,
        decltype(tok_r) &&tok_r);
    ~FunctionParameterGroupAst() override;
    auto to_rust() const -> std::string override;
};
