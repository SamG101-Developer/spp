module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_comp_ast;
import spp.asts.generic_parameter_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterCompAst;
    SPP_EXP_CLS struct IdentifierAst;
}


SPP_EXP_CLS struct spp::asts::GenericParameterCompAst : GenericParameterAst {
    std::unique_ptr<IdentifierAst> name;
    std::unique_ptr<TypeAst> type;

    explicit GenericParameterCompAst(
        decltype(name) &&name,
        decltype(type) &&type);
    ~GenericParameterCompAst() override;
    auto to_rust() const -> std::string override;
};
