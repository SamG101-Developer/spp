#pragma once
#include <spp/asts/inner_scope_ast.hpp>
#include <spp/asts/primary_expression_ast.hpp>


template <typename T>
struct spp::asts::InnerScopeExpressionAst final : InnerScopeAst<T>, PrimaryExpressionAst {
    using InnerScopeAst<T>::InnerScopeAst;

    auto clone() const -> std::unique_ptr<Ast> override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
