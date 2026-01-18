module;
#include <spp/analyse/macros.hpp>

module spp.asts.class_implementation_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.asts.identifier_ast;
import spp.asts.class_attribute_ast;
import spp.asts.utils.ast_utils;
import spp.asts.utils.ast_utils;
import genex;


spp::asts::ClassImplementationAst::~ClassImplementationAst() = default;


auto spp::asts::ClassImplementationAst::clone() const
    -> std::unique_ptr<Ast> {
    auto *c = InnerScopeAst::clone().release()->to<InnerScopeAst>();
    return std::make_unique<ClassImplementationAst>(
        std::move(c->tok_l),
        std::move(c->members),
        std::move(c->tok_r));
}


auto spp::asts::ClassImplementationAst::new_empty()
    -> std::unique_ptr<ClassImplementationAst> {
    return std::make_unique<ClassImplementationAst>();
}


auto spp::asts::ClassImplementationAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    for (auto const &m: members) {
        m->stage_1_pre_process(ctx);
    }
}


auto spp::asts::ClassImplementationAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Generate scopes for each member.
    for (auto const &m : members) {
        m->stage_2_gen_top_level_scopes(sm, meta);
    }
}


auto spp::asts::ClassImplementationAst::stage_3_gen_top_level_aliases(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Generate aliases for each member.
    for (auto const &m : members) {
        m->stage_3_gen_top_level_aliases(sm, meta);
    }
}


auto spp::asts::ClassImplementationAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Qualify types for each member.
    for (auto const &m : members) {
        m->stage_4_qualify_types(sm, meta);
    }
}


auto spp::asts::ClassImplementationAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Load super scopes for each member.
    for (auto const &m : members) {
        m->stage_5_load_super_scopes(sm, meta);
    }
}


auto spp::asts::ClassImplementationAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Pre-analyse semantics for each member.
    for (auto const &m : members) {
        m->stage_6_pre_analyse_semantics(sm, meta);
    }

    // Ensure there are no duplicate member names. This needs to be done before semantic analysis as other ASTs might
    // try reading a duplicate attribute before an error is raised.
    const auto duplicates = members
        | genex::views::transform([](auto &&x) { return x->template to<ClassAttributeAst>()->name.get(); })
        | genex::to<std::vector>()
        | genex::views::duplicates({}, genex::meta::deref)
        | genex::to<std::vector>();

    if (not duplicates.empty()) {
        analyse::errors::SemanticErrorBuilder<analyse::errors::SppIdentifierDuplicateError>()
            .with_args(*duplicates[0], *duplicates[1], "attribute")
            .raises_from(sm->current_scope);
    }
}


auto spp::asts::ClassImplementationAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse semantics for each member.
    for (auto const &m : members) {
        m->stage_7_analyse_semantics(sm, meta);
    }
}


auto spp::asts::ClassImplementationAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check memory for each member.
    for (auto const &m : members) {
        m->stage_8_check_memory(sm, meta);
    }
}


auto spp::asts::ClassImplementationAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate code for each member.
    for (auto const &m : members) {
        m->stage_11_code_gen_2(sm, meta, ctx);
    }
    return nullptr;
}
