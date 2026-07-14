module;
#include <spp/macros.hpp>

module spp.asts.sup_implementation_ast;
import spp.asts.sup_member_ast;
import spp.asts.utils.ast_utils;
import spp.asts.token_ast;
import spp.utils.algorithms;

SPP_MOD_BEGIN
auto spp::asts::SupImplementationAst::NewEmpty()
    -> Unique<SupImplementationAst> {
    // Empty ast.
    return MakeUnique<SupImplementationAst>();
}

spp::asts::SupImplementationAst::~SupImplementationAst() = default;

auto spp::asts::SupImplementationAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<SupImplementationAst>(
        AstClone(TokL), AstCloneVec(Members), AstClone(TokR));
}

auto spp::asts::SupImplementationAst::Stage1_PreProcess(
    Ast *ctx)
    -> void {
    // Shift to members (copy because function pre-processing edits this module's member).
    const auto members_ptrs = Members | spp::views::ptr | std::ranges::to<Vec>();
    for (auto *m : members_ptrs) { m->Stage1_PreProcess(ctx); }
}

auto spp::asts::SupImplementationAst::Stage2_GenTopLvlScopes(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Forward to members.
    for (auto const &m : Members) { m->Stage2_GenTopLvlScopes(sm, meta); }
}

auto spp::asts::SupImplementationAst::Stage3_GenTopLvlAliases(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Forward to members.
    for (auto const &m : Members) { m->Stage3_GenTopLvlAliases(sm, meta); }
}

auto spp::asts::SupImplementationAst::Stage4_QualifyTypes(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Forward to members.
    for (auto const &m : Members) { m->Stage4_QualifyTypes(sm, meta); }
}

auto spp::asts::SupImplementationAst::Stage5_LoadSupScopes(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Forward to members.
    for (auto const &m : Members) { m->Stage5_LoadSupScopes(sm, meta); }
}

auto spp::asts::SupImplementationAst::Stage6_PreAnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Forward to members.
    for (auto const &m : Members) { m->Stage6_PreAnalyseSemantics(sm, meta); }
}

auto spp::asts::SupImplementationAst::Stage7_AnalyseSemantics(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Forward to members.
    for (auto const &m : Members) { m->Stage7_AnalyseSemantics(sm, meta); }
}

auto spp::asts::SupImplementationAst::Stage8_CheckMemory(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Forward to members.
    for (auto const &m : Members) { m->Stage8_CheckMemory(sm, meta); }
}

auto spp::asts::SupImplementationAst::Stage9_CompTimeResolve(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta)
    -> void {
    // Forward to members.
    for (auto const &m : Members) { m->Stage9_CompTimeResolve(sm, meta); }
}

auto spp::asts::SupImplementationAst::Stage10_PreCodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Forward to members.
    for (auto const &m : Members) { m->Stage10_PreCodeGen(sm, meta, ctx); }
    return nullptr;
}

auto spp::asts::SupImplementationAst::Stage11_CodeGen(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Forward to members.
    for (auto const &m : Members) { m->Stage11_CodeGen(sm, meta, ctx); }
    return nullptr;
}

SPP_MOD_END
