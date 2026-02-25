module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_required_ast;
import spp.asts.function_parameter_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct FunctionParameterRequiredAst;
}


SPP_EXP_CLS struct spp::asts::FunctionParameterRequiredAst final : FunctionParameterAst {
    FunctionParameterRequiredAst(
        decltype(var) &&var,
        decltype(tok_colon) &&tok_colon,
        decltype(type) type);
    ~FunctionParameterRequiredAst() override;
    auto to_rust() const -> std::string override;
};
