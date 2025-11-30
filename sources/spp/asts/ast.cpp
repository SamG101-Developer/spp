module spp.asts.ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import std;


spp::asts::Ast::Ast() = default;


spp::asts::Ast::~Ast() = default;


auto spp::asts::Ast::size() const
    -> std::size_t {
    return pos_end() - pos_start();
}


auto spp::asts::Ast::stage_1_pre_process(
    Ast *ctx)
    -> void {
    m_ctx = ctx;
}


auto spp::asts::Ast::stage_2_gen_top_level_scopes(
    ScopeManager *sm,
    CompilerMetaData *)
    -> void {
    m_scope = sm->current_scope;
}
