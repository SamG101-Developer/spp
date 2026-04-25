module;
#include <spp/macros.hpp>

module spp.asts.ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import std;


SPP_MOD_BEGIN
spp::asts::Ast::Ast() = default;


spp::asts::Ast::~Ast() = default;


auto spp::asts::Ast::size() const
    -> std::size_t {
    return pos_end() - pos_start();
}


auto spp::asts::Ast::ankerl_hash() const
    -> std::size_t {
    return 0uz;
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


auto spp::asts::Ast::to_string() const
    -> std::string {
    return static_cast<std::string>(*this);
}


auto spp::asts::Ast::get_ast_scope() const
    -> analyse::scopes::Scope* {
    return m_scope;
}


auto spp::asts::Ast::get_ast_ctx() const
    -> Ast* {
    return m_ctx;
}


auto spp::asts::Ast::set_ast_scope(
    analyse::scopes::Scope *scope)
    -> void {
    m_scope = scope;
}


auto spp::asts::Ast::set_ast_ctx(
    Ast *ctx)
    -> void {
    m_ctx = ctx;
}

SPP_MOD_END
