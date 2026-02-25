module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_tuple_ast;
import spp.asts.local_variable_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LocalVariableDestructureTupleAst;
}


SPP_EXP_CLS struct spp::asts::LocalVariableDestructureTupleAst final : LocalVariableAst {
    std::vector<std::unique_ptr<LocalVariableAst>> elems;

    explicit LocalVariableDestructureTupleAst(
        decltype(elems) &&elems);
    ~LocalVariableDestructureTupleAst() override;
    auto to_rust() const -> std::string override;
};
