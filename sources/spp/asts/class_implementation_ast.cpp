module;
#include <spp/analyse/macros.hpp>
#include <spp/macros.hpp>

module spp.asts.class_implementation_ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope_manager;
import spp.asts.identifier_ast;
import spp.asts.class_attribute_ast;
import spp.asts.token_ast;
import spp.asts.utils.ast_utils;
import genex;

SPP_MOD_BEGIN
auto spp::asts::ClassImplementationAst::NewEmpty()
    -> Unique<ClassImplementationAst> {
    // Empty AST.
    return MakeUnique<ClassImplementationAst>();
}

spp::asts::ClassImplementationAst::~ClassImplementationAst() = default;

auto spp::asts::ClassImplementationAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<ClassImplementationAst>(
        AstClone(TokL), AstCloneVec(Members), AstClone(TokR));
}

auto spp::asts::ClassImplementationAst::Stage1_PreProcess(
    Ast *ctx)
    -> void {
    // Preprocess members.
    for (auto const &m : Members) { m->Stage1_PreProcess(ctx); }
}

auto spp::asts::ClassImplementationAst::Stage2_GenTopLvlScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Generate scopes for each member.
    for (auto const &m : Members) { m->Stage2_GenTopLvlScopes(sm, meta); }
}

auto spp::asts::ClassImplementationAst::Stage3_GenTopLvlAliases(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Generate aliases for each member.
    for (auto const &m : Members) { m->Stage3_GenTopLvlAliases(sm, meta); }
}

auto spp::asts::ClassImplementationAst::Stage4_QualifyTypes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Qualify types for each member.
    for (auto const &m : Members) { m->Stage4_QualifyTypes(sm, meta); }
}

auto spp::asts::ClassImplementationAst::Stage5_LoadSupScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Load super scopes for each member.
    for (auto const &m : Members) { m->Stage5_LoadSupScopes(sm, meta); }
}

auto spp::asts::ClassImplementationAst::Stage6_PreAnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Pre-analyse semantics for each member.
    using analyse::errors::SppIdentifierDuplicateError;
    for (auto const &m : Members) { m->Stage6_PreAnalyseSemantics(sm, meta); }

    // Ensure there are no duplicate member names. This needs to be done before semantic analysis as other ASTs might
    // try reading a duplicate attribute before an error is raised.
    const auto duplicates = Members
        | genex::views::transform([](auto const &x) { return x->template To<ClassAttributeAst>()->Name.get(); })
        | genex::to<Vec>()
        | genex::views::duplicates({}, genex::meta::deref)
        | genex::to<Vec>();

    RaiseIf<SppIdentifierDuplicateError>(
        not duplicates.IsEmpty(),
        {sm->CurrentScope}, ERR_ARGS(*duplicates[0], *duplicates[1], "attribute"));
}

auto spp::asts::ClassImplementationAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Analyse semantics for each member.
    for (auto const &m : Members) { m->Stage7_AnalyseSemantics(sm, meta); }
}

auto spp::asts::ClassImplementationAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Check memory for each member.
    for (auto const &m : Members) { m->Stage8_CheckMemory(sm, meta); }
}

auto spp::asts::ClassImplementationAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Compile-time resolution for each member.
    for (auto const &m : Members) { m->Stage9_CompTimeResolve(sm, meta); }
}

auto spp::asts::ClassImplementationAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Generate code for each member.
    for (auto const &m : Members) { m->Stage11_CodeGen(sm, meta, ctx); }
    return nullptr;
}

SPP_MOD_END
