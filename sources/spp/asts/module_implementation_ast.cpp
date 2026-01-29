module;
#include <spp/macros.hpp>

module spp.asts.module_implementation_ast;
import spp.asts.module_member_ast;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_ctx;
import genex;


spp::asts::ModuleImplementationAst::ModuleImplementationAst(
    decltype(members) &&members) :
    members(std::move(members)) {
}


spp::asts::ModuleImplementationAst::~ModuleImplementationAst() = default;


auto spp::asts::ModuleImplementationAst::pos_start() const
    -> std::size_t {
    return members.empty() ? 0 : members.front()->pos_start();
}


auto spp::asts::ModuleImplementationAst::pos_end() const
    -> std::size_t {
    return members.empty() ? 0 : members.back()->pos_end();
}


auto spp::asts::ModuleImplementationAst::clone() const
    -> std::unique_ptr<Ast> {
    return std::make_unique<ModuleImplementationAst>(
        ast_clone_vec(members));
}


spp::asts::ModuleImplementationAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(members, "\n");
    SPP_STRING_END;
}


auto spp::asts::ModuleImplementationAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // Shift to members.
    for (auto *member: members | genex::views::ptr | genex::to<std::vector>()) {
        member->stage_1_pre_process(ctx);
    }
}


auto spp::asts::ModuleImplementationAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Shift to members.
    for (auto const &member : members) {
        member->stage_2_gen_top_level_scopes(sm, meta);
    }
}


auto spp::asts::ModuleImplementationAst::stage_3_gen_top_level_aliases(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Shift to members.
    for (auto const &member : members) {
        member->stage_3_gen_top_level_aliases(sm, meta);
    }
}


auto spp::asts::ModuleImplementationAst::stage_4_qualify_types(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Shift to members.
    for (auto const &member : members) {
        member->stage_4_qualify_types(sm, meta);
    }
}


auto spp::asts::ModuleImplementationAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Shift to members.
    for (auto const &member : members) {
        member->stage_5_load_super_scopes(sm, meta);
    }
}


auto spp::asts::ModuleImplementationAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Shift to members.
    for (auto const &member : members) {
        member->stage_6_pre_analyse_semantics(sm, meta);
    }
}


auto spp::asts::ModuleImplementationAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Shift to members.
    for (auto const &member : members) {
        member->stage_7_analyse_semantics(sm, meta);
    }
}


auto spp::asts::ModuleImplementationAst::stage_8_check_memory(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Shift to members.
    for (auto const &member : members) {
        member->stage_8_check_memory(sm, meta);
    }
}


auto spp::asts::ModuleImplementationAst::stage_9_comptime_resolution(
    ScopeManager *sm,
    CompilerMetaData *meta)
    -> void {
    // Shift to members, and return nullptr as this value is never used.
    for (auto const &member : members) {
        member->stage_9_comptime_resolution(sm, meta);
    }
}


auto spp::asts::ModuleImplementationAst::stage_10_code_gen_1(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Shift to members.
    for (auto const &member : members) {
        member->stage_10_code_gen_1(sm, meta, ctx);
    }
    return nullptr;
}


auto spp::asts::ModuleImplementationAst::stage_11_code_gen_2(
    ScopeManager *sm,
    CompilerMetaData *meta,
    codegen::LLvmCtx *ctx)
    -> llvm::Value* {
    // Shift to members.
    for (auto const &member : members) {
        member->stage_11_code_gen_2(sm, meta, ctx);
    }
    return nullptr;
}
