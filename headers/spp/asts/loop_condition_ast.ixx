module;
#include <spp/macros.hpp>

export module spp.asts.loop_condition_ast;
import spp.asts.ast;


SPP_EXP_CLS struct spp::asts::LoopConditionAst : virtual Ast {
    using Ast::Ast;

    ~LoopConditionAst() override;
};
