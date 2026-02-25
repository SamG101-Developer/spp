module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_comp_variadic_ast;
import spp.asts.generic_parameter_comp_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterCompVariadicAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::GenericParameterCompVariadicAst final : GenericParameterCompAst {
    GenericParameterCompVariadicAst(
        decltype(name) &&name,
        decltype(type) &&type);
    ~GenericParameterCompVariadicAst() override;
    auto to_rust() const -> std::string override;
};
