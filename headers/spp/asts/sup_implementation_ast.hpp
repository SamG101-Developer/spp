#ifndef SUP_IMPLEMENTATION_AST_HPP
#define SUP_IMPLEMENTATION_AST_HPP

#include <spp/asts/inner_scope_ast.hpp>
#include <spp/asts/_fwd.hpp>

namespace spp::parse {
    class ParserSpp;
}


struct spp::asts::SupImplementationAst final : InnerScopeAst<std::unique_ptr<SupMemberAst>> {
    friend class parse::ParserSpp;
    using InnerScopeAst::InnerScopeAst;

    ~SupImplementationAst() override;

private:
    static auto new_empty() -> std::unique_ptr<SupImplementationAst>;

public:
    auto stage_1_pre_process(Ast *ctx) -> void override;

    auto stage_2_gen_top_level_scopes(ScopeManager *sm, mixins::CompilerMetaData *) -> void override;

    auto stage_3_gen_top_level_aliases(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_4_qualify_types(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_5_load_super_scopes(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_6_pre_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_8_check_memory(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;
};


#endif //SUP_IMPLEMENTATION_AST_HPP
