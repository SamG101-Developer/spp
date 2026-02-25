module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_group_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
}


SPP_EXP_CLS struct spp::asts::GenericParameterGroupAst final : Ast {
    std::vector<std::unique_ptr<GenericParameterAst>> params;

    explicit GenericParameterGroupAst(
        decltype(params) &&params);
    ~GenericParameterGroupAst() override;
    auto to_rust() const -> std::string override;
};
