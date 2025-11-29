module;
#include <spp/macros.hpp>

export module spp.asts.loop_condition_ast;
import spp.asts.ast;

namespace spp::asts {
    SPP_EXP_CLS struct LoopConditionAst;
}


SPP_EXP_CLS struct spp::asts::LoopConditionAst : virtual Ast {
    LoopConditionAst() = default;

    ~LoopConditionAst() override;
};
