module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_group_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericArgumentAst;
}


SPP_EXP_CLS struct spp::asts::GenericArgumentGroupAst final : Ast {
    std::vector<std::unique_ptr<GenericArgumentAst>> args;

    GenericArgumentGroupAst(
        decltype(args) &&args);
    ~GenericArgumentGroupAst() override;
    auto to_rust() const -> std::string override;
};
