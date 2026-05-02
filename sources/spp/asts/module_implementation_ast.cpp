module;
#include <spp/macros.hpp>

module spp.asts.module_implementation_ast;
import spp.asts.module_member_ast;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_ctx;
import genex;

SPP_MOD_BEGIN
spp::asts::ModuleImplementationAst::ModuleImplementationAst(
    decltype(Members) &&members) :
    Members(std::move(members)) {
}

spp::asts::ModuleImplementationAst::~ModuleImplementationAst() = default;

auto spp::asts::ModuleImplementationAst::PosStart() const
    -> std::size_t {
    // Use the first member.
    return Members.IsEmpty() ? 0 : Members.Front()->PosStart();
}

auto spp::asts::ModuleImplementationAst::PosEnd() const
    -> std::size_t {
    // Use the last member.
    return Members.IsEmpty() ? 0 : Members.Back()->PosEnd();
}

auto spp::asts::ModuleImplementationAst::Clone() const
    -> Unique<Ast> {
    // Clone all the members of the ast.
    return MakeUnique<ModuleImplementationAst>(
        AstCloneVec(Members));
}

auto spp::asts::ModuleImplementationAst::ToString() const
    -> Str {
    SPP_STRING_START;
    SPP_STRING_EXTEND(Members, "\n");
    SPP_STRING_END;
}

auto spp::asts::ModuleImplementationAst::Stage1_PreProcess(
    Ast *ctx)
    -> void {
    // Shift to members (copy because function pre-processing edits this module's member).
    const auto members_ptrs = Members | genex::views::ptr | genex::to<Vec>();
    for (auto *member : members_ptrs) { member->Stage1_PreProcess(ctx); }
}

auto spp::asts::ModuleImplementationAst::Stage2_GenTopLvlScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Shift to members.
    for (auto const &member : Members) { member->Stage2_GenTopLvlScopes(sm, meta); }
}

auto spp::asts::ModuleImplementationAst::Stage3_GenTopLvlAliases(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Shift to members.
    for (auto const &member : Members) { member->Stage3_GenTopLvlAliases(sm, meta); }
}

auto spp::asts::ModuleImplementationAst::Stage4_QualifyTypes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Shift to members.
    for (auto const &member : Members) { member->Stage4_QualifyTypes(sm, meta); }
}

auto spp::asts::ModuleImplementationAst::Stage5_LoadSupScopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Shift to members.
    for (auto const &member : Members) { member->Stage5_LoadSupScopes(sm, meta); }
}

auto spp::asts::ModuleImplementationAst::Stage6_PreAnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Shift to members.
    for (auto const &member : Members) { member->Stage6_PreAnalyseSemantics(sm, meta); }
}

auto spp::asts::ModuleImplementationAst::Stage7_AnalyseSemantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Shift to members.
    for (auto const &member : Members) { member->Stage7_AnalyseSemantics(sm, meta); }
}

auto spp::asts::ModuleImplementationAst::Stage8_CheckMemory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Shift to members.
    for (auto const &member : Members) { member->Stage8_CheckMemory(sm, meta); }
}

auto spp::asts::ModuleImplementationAst::Stage9_CompTimeResolve(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Shift to members, and return nullptr as this value is never used.
    for (auto const &member : Members) { member->Stage9_CompTimeResolve(sm, meta); }
}

auto spp::asts::ModuleImplementationAst::Stage10_PreCodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Shift to members.
    for (auto const &member : Members) { member->Stage10_PreCodeGen(sm, meta, ctx); }
    return nullptr;
}

auto spp::asts::ModuleImplementationAst::Stage11_CodeGen(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Shift to members.
    for (auto const &member : Members) { member->Stage11_CodeGen(sm, meta, ctx); }
    return nullptr;
}

SPP_MOD_END
