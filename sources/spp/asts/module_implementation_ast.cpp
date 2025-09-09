#include <spp/asts/module_implementation_ast.hpp>
#include <spp/asts/module_member_ast.hpp>
#include <spp/pch.hpp>

#include <genex/views/for_each.hpp>


spp::asts::ModuleImplementationAst::ModuleImplementationAst(
    decltype(members) &&members) :
    members(std::move(members)) {
}


spp::asts::ModuleImplementationAst::~ModuleImplementationAst() = default;


auto spp::asts::ModuleImplementationAst::pos_start() const -> std::size_t {
    return members.empty() ? 0 : members.front()->pos_start();
}


auto spp::asts::ModuleImplementationAst::pos_end() const -> std::size_t {
    return members.empty() ? 0 : members.back()->pos_end();
}


auto spp::asts::ModuleImplementationAst::clone() const -> std::unique_ptr<Ast> {
    return std::make_unique<ModuleImplementationAst>(
        ast_clone_vec(members));
}


spp::asts::ModuleImplementationAst::operator std::string() const {
    SPP_STRING_START;
    SPP_STRING_EXTEND(members);
    SPP_STRING_END;
}


auto spp::asts::ModuleImplementationAst::print(meta::AstPrinter &printer) const -> std::string {
    SPP_PRINT_START;
    SPP_PRINT_EXTEND(members);
    SPP_PRINT_END;
}


auto spp::asts::ModuleImplementationAst::stage_1_pre_process(
    Ast *ctx)
    -> void {
    // Shift to members.
    members | genex::views::for_each([ctx](auto &&x) { x->stage_1_pre_process(ctx); });
}


auto spp::asts::ModuleImplementationAst::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Shift to members.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_2_gen_top_level_scopes(sm, meta); });
}


auto spp::asts::ModuleImplementationAst::stage_3_gen_top_level_aliases(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Shift to members.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_3_gen_top_level_aliases(sm, meta); });
}


auto spp::asts::ModuleImplementationAst::stage_4_qualify_types(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Shift to members.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_4_qualify_types(sm, meta); });
}


auto spp::asts::ModuleImplementationAst::stage_5_load_super_scopes(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Shift to members.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_5_load_super_scopes(sm, meta); });
}


auto spp::asts::ModuleImplementationAst::stage_6_pre_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Shift to members.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_6_pre_analyse_semantics(sm, meta); });
}


auto spp::asts::ModuleImplementationAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Shift to members.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_7_analyse_semantics(sm, meta); });
}


auto spp::asts::ModuleImplementationAst::stage_8_check_memory(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Shift to members.
    members | genex::views::for_each([sm, meta](auto &&x) { x->stage_8_check_memory(sm, meta); });
}
