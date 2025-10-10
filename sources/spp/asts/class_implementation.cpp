#include <spp/analyse/errors/semantic_error.hpp>
#include <spp/analyse/errors/semantic_error_builder.hpp>
#include <spp/analyse/scopes/scope_manager.hpp>
#include <spp/asts/class_attribute_ast.hpp>
#include <spp/asts/class_implementation_ast.hpp>
#include <spp/asts/class_member_ast.hpp>
#include <spp/asts/identifier_ast.hpp>
#include <spp/asts/token_ast.hpp>

#include <genex/views/for_each.hpp>
#include <genex/views/duplicates.hpp>
#include <genex/views/materialize.hpp>
#include <genex/to_container.hpp>


auto spp::asts::ClassImplementationAst::new_empty()
    -> std::unique_ptr<ClassImplementationAst> {
    return std::make_unique<ClassImplementationAst>();
}


spp::asts::ClassImplementationAst::~ClassImplementationAst() = default;


auto spp::asts::ClassImplementationAst::clone() const
    -> std::unique_ptr<Ast> {
    auto *c = ast_cast<InnerScopeAst>(InnerScopeAst::clone().release());
    return std::make_unique<ClassImplementationAst>(
        std::move(c->tok_l),
        std::move(c->members),
        std::move(c->tok_r));
}


auto spp::asts::ClassImplementationAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    members | genex::views::for_each([ctx](auto &&x) { x->stage_1_pre_process(ctx); });
}


auto spp::asts::ClassImplementationAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Generate scopes for each member.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_2_gen_top_level_scopes(sm, meta); });
}


auto spp::asts::ClassImplementationAst::stage_3_gen_top_level_aliases(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Generate aliases for each member.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_3_gen_top_level_aliases(sm, meta); });
}


auto spp::asts::ClassImplementationAst::stage_4_qualify_types(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Qualify types for each member.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_4_qualify_types(sm, meta); });
}


auto spp::asts::ClassImplementationAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Load super scopes for each member.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_5_load_super_scopes(sm, meta); });
}


auto spp::asts::ClassImplementationAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Pre-analyse semantics for each member.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_6_pre_analyse_semantics(sm, meta); });

    // Ensure there are no duplicate member names. This needs to be done before semantic analysis as other ASTs might
    // try reading a duplicate attribute before an error is raised.
    const auto duplicates = members
        | genex::views::transform([](auto &&x) { return ast_cast<ClassAttributeAst>(*x).name.get(); })
        | genex::views::materialize
        | genex::views::duplicates({}, genex::meta::deref)
        | genex::to<std::vector>();

    if (not duplicates.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppIdentifierDuplicateError>().with_args(
            *duplicates[0], *duplicates[1], "attribute").with_scopes({sm->current_scope}).raise();
    }
}


auto spp::asts::ClassImplementationAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse semantics for each member.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_7_analyse_semantics(sm, meta); });
}


auto spp::asts::ClassImplementationAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Check memory for each member.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_8_check_memory(sm, meta); });
}
