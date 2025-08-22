#include <spp/asts/sup_implementation_ast.hpp>
#include <spp/asts/sup_member_ast.hpp>
#include <spp/asts/token_ast.hpp>

#include <genex/views/for_each.hpp>


auto spp::asts::SupImplementationAst::new_empty() -> std::unique_ptr<SupImplementationAst> {
    return std::make_unique<SupImplementationAst>();
}


auto spp::asts::SupImplementationAst::stage_1_pre_process(Ast *ctx) -> void {
    members | genex::views::for_each([&ctx](auto &&m) { m->stage_1_pre_process(ctx); });
}


auto spp::asts::SupImplementationAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward to members.
    members | genex::views::for_each([sm, meta](auto &&m) { m->stage_2_gen_top_level_scopes(sm, meta); });
}


auto spp::asts::SupImplementationAst::stage_3_gen_top_level_aliases(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward to members.
    members | genex::views::for_each([sm, meta](auto &&m) { m->stage_3_gen_top_level_aliases(sm, meta); });
}


auto spp::asts::SupImplementationAst::stage_4_qualify_types(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward to members.
    members | genex::views::for_each([sm, meta](auto &&m) { m->stage_4_qualify_types(sm, meta); });
}


auto spp::asts::SupImplementationAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward to members.
    members | genex::views::for_each([sm, meta](auto &&m) { m->stage_5_load_super_scopes(sm, meta); });
}


auto spp::asts::SupImplementationAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward to members.
    members | genex::views::for_each([sm, meta](auto &&m) { m->stage_6_pre_analyse_semantics(sm, meta); });
}


auto spp::asts::SupImplementationAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward to members.
    members | genex::views::for_each([sm, meta](auto &&m) { m->stage_7_analyse_semantics(sm, meta); });
}


auto spp::asts::SupImplementationAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Forward to members.
    members | genex::views::for_each([sm, meta](auto &&m) { m->stage_8_check_memory(sm, meta); });
}
