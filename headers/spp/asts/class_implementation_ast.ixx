module;
#include <spp/macros.hpp>

export module spp.asts.class_implementation_ast;
import spp.asts.inner_scope_ast;

import std;

/// @cond
namespace spp::parse {
    class ParserSpp;
}

/// @endcond


SPP_EXP_CLS struct spp::asts::ClassImplementationAst final : InnerScopeAst<std::unique_ptr<ClassMemberAst>> {
    friend class parse::ParserSpp;
    using InnerScopeAst::InnerScopeAst;

    ~ClassImplementationAst() override;

    auto clone() const -> std::unique_ptr<Ast> override;

public:
    static auto new_empty() -> std::unique_ptr<ClassImplementationAst>;

    auto stage_1_pre_process(Ast *ctx) -> void override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, CompilerMetaData *) -> void override;

    auto stage_3_gen_top_level_aliases(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_5_load_super_scopes(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_6_pre_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
