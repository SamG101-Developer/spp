module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_skip_multiple_arguments_ast;
import spp.asts.local_variable_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LocalVariableDestructureSkipMultipleArgumentsAst;
    SPP_EXP_CLS struct LocalVariableSingleIdentifierAst;
}


SPP_EXP_CLS struct spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst final : LocalVariableAst {
    std::unique_ptr<LocalVariableSingleIdentifierAst> binding;

    explicit LocalVariableDestructureSkipMultipleArgumentsAst(
        decltype(binding) &&binding);
    ~LocalVariableDestructureSkipMultipleArgumentsAst() override;
    auto to_rust() const -> std::string override;
};
