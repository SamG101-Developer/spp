#pragma once
#include <spp/asts/ast.hpp>


struct spp::asts::LoopConditionAst : virtual Ast {
    using Ast::Ast;

    ~LoopConditionAst() override;
};
