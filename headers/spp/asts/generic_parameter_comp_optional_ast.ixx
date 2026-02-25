module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_comp_optional_ast;
import spp.asts.generic_parameter_comp_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct GenericParameterCompOptionalAst;
}


SPP_EXP_CLS struct spp::asts::GenericParameterCompOptionalAst final : GenericParameterCompAst {
    std::unique_ptr<ExpressionAst> default_val;

    GenericParameterCompOptionalAst(
        decltype(name) &&name,
        decltype(type) &&type,
        decltype(default_val) &&default_val);
    ~GenericParameterCompOptionalAst() override;
    auto to_rust() const -> std::string override;
};
