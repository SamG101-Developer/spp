module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_attribute_binding_ast;
import spp.asts.local_variable_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct LocalVariableDestructureAttributeBindingAst;
    SPP_EXP_CLS struct IdentifierAst;
}


SPP_EXP_CLS struct spp::asts::LocalVariableDestructureAttributeBindingAst final : LocalVariableAst {
    std::unique_ptr<IdentifierAst> name;
    std::unique_ptr<LocalVariableAst> val;

    explicit LocalVariableDestructureAttributeBindingAst(
        decltype(name) &&name,
        decltype(val) &&val);
    ~LocalVariableDestructureAttributeBindingAst() override;
    auto to_rust() const -> std::string override;
};
