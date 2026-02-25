module;
#include <spp/macros.hpp>

export module spp.asts.generic_parameter_type_ast;
import spp.asts.generic_parameter_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericParameterTypeAst;
    SPP_EXP_CLS struct GenericParameterTypeInlineConstraintsAst;
}


SPP_EXP_CLS struct spp::asts::GenericParameterTypeAst : GenericParameterAst {
    std::unique_ptr<TypeAst> name;
    std::unique_ptr<GenericParameterTypeInlineConstraintsAst> constraints;

    explicit GenericParameterTypeAst(
        decltype(name) name,
        decltype(constraints) &&constraints);
    ~GenericParameterTypeAst() override;
    auto to_rust() const -> std::string override;
};
