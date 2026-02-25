module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_skip_single_argument_ast;
import spp.asts.local_variable_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct LocalVariableDestructureSkipSingleArgumentAst;
}


SPP_EXP_CLS struct spp::asts::LocalVariableDestructureSkipSingleArgumentAst final : LocalVariableAst {
    explicit LocalVariableDestructureSkipSingleArgumentAst();
    ~LocalVariableDestructureSkipSingleArgumentAst() override;
    auto to_rust() const -> std::string override;
};
