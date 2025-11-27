module;
#include <spp/macros.hpp>

export module spp.asts.subroutine_prototype_ast;
import spp.asts.function_prototype_ast;

import std;


SPP_EXP_CLS struct spp::asts::SubroutinePrototypeAst final : FunctionPrototypeAst {
    using FunctionPrototypeAst::FunctionPrototypeAst;

    ~SubroutinePrototypeAst() override;

    auto clone() const -> std::unique_ptr<Ast> override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
