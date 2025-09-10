#pragma once
#include <spp/asts/function_prototype_ast.hpp>


struct spp::asts::SubroutinePrototypeAst final : FunctionPrototypeAst {
    using FunctionPrototypeAst::FunctionPrototypeAst;

    ~SubroutinePrototypeAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
