module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_array_ast;
import spp.asts.local_variable_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LocalVariableDestructureArrayAst;
}


SPP_EXP_CLS struct spp::asts::LocalVariableDestructureArrayAst final : LocalVariableAst {
    std::vector<std::unique_ptr<LocalVariableAst>> elems;

    LocalVariableDestructureArrayAst(
        decltype(elems) &&elems);
    ~LocalVariableDestructureArrayAst() override;
    auto to_rust() const -> std::string override;
};
