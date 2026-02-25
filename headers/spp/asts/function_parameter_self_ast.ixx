module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_self_ast;
import spp.asts.function_parameter_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct FunctionParameterSelfAst;
}


SPP_EXP_CLS struct spp::asts::FunctionParameterSelfAst final : FunctionParameterAst {
    std::unique_ptr<ConventionAst> conv;

    FunctionParameterSelfAst(
        decltype(conv) &&conv,
        decltype(var) &&var);
    ~FunctionParameterSelfAst() override;
    auto to_rust() const -> std::string override;
};
