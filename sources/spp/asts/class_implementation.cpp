#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/asts/class_implementation_ast.hpp>
#include <spp/asts/class_member_ast.hpp>
#include <spp/asts/token_ast.hpp>

#include <genex/views/for_each.hpp>
#include <genex/views/duplicates.hpp>
#include <genex/views/to.hpp>

#include "spp/analyse/scopes/scope_manager.hpp"


auto spp::asts::ClassImplementationAst::new_empty() -> std::unique_ptr<ClassImplementationAst> {
    return std::make_unique<ClassImplementationAst>();
}


auto spp::asts::ClassImplementationAst::stage_1_pre_process(Ast *ctx) -> void {
    members | genex::views::for_each([ctx](auto &&x) { x->stage_1_pre_process(ctx); });
}


auto spp::asts::ClassImplementationAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Generate scopes for each member.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_2_gen_top_level_scopes(sm, meta); });
}


auto spp::asts::ClassImplementationAst::stage_3_gen_top_level_aliases(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Generate aliases for each member.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_3_gen_top_level_aliases(sm, meta); });
}


auto spp::asts::ClassImplementationAst::stage_4_qualify_types(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Qualify types for each member.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_4_qualify_types(sm, meta); });
}


auto spp::asts::ClassImplementationAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Load super scopes for each member.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_5_load_super_scopes(sm, meta); });
}


auto spp::asts::ClassImplementationAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Pre-analyse semantics for each member.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_6_pre_analyse_semantics(sm, meta); });

    // Ensure there are no duplicate member names. This needs to be done before semantic analysis as other ASTs might
    // try reading a duplicate attribute before an error is raised.
    const auto duplicates = members
        | genex::views::map([](auto &&x) { return ast_cast<ClassAttributeAst>(*x).name.get(); })
        | genex::views::duplicates()
        | genex::views::to<std::vector>();

    if (not duplicates.empty()) {
        analyse::errors::SppIdentifierDuplicateError(duplicates[0], duplicates[1], "attribute")
            .scopes({sm->current_scope})
            .raise();
    }
}


auto spp::asts::ClassImplementationAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Analyse semantics for each member.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_7_analyse_semantics(sm, meta); });
}


auto spp::asts::ClassImplementationAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta) -> void {
    // Check memory for each member.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_8_check_memory(sm, meta); });
}
