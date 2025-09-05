#pragma once
#include <spp/asts/function_prototype_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::CoroutinePrototypeAst final : FunctionPrototypeAst {
    using FunctionPrototypeAst::FunctionPrototypeAst;

    ~CoroutinePrototypeAst() override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};
