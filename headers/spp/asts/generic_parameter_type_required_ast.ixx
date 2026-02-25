module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_type_required_ast;
import spp.asts.generic_parameter_type_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterTypeRequiredAst;
}


SPP_EXP_CLS struct spp::asts::GenericParameterTypeRequiredAst final : GenericParameterTypeAst {
    explicit GenericParameterTypeRequiredAst(
        decltype(name) &&name,
        decltype(constraints) &&constraints);
    ~GenericParameterTypeRequiredAst() override;
    auto to_rust() const -> std::string override;
};
