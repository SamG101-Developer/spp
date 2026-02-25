module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_object_ast;
import spp.asts.local_variable_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LocalVariableDestructureObjectAst;
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::LocalVariableDestructureObjectAst final : LocalVariableAst {
    std::unique_ptr<TypeAst> type;
    std::vector<std::unique_ptr<LocalVariableAst>> elems;

    explicit LocalVariableDestructureObjectAst(
        decltype(type) &&type,
        decltype(elems) &&elems);
    ~LocalVariableDestructureObjectAst() override;
    auto to_rust() const -> std::string override;
};
