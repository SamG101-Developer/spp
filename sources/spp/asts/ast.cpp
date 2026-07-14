module;
#include <spp/macros.hpp>

module spp.asts.ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import std;

SPP_MOD_BEGIN
spp::asts::Ast::Ast() = default;

spp::asts::Ast::~Ast() = default;

auto spp::asts::Ast::Size() const
    -> std::size_t {
    return PosEnd() - PosStart();
}

auto spp::asts::Ast::AnkerlHash() const
    -> std::size_t {
    return 0uz;
}

auto spp::asts::Ast::Stage1_PreProcess(
    Ast *ctx)
    -> void {
    _Ctx = ctx;
}

auto spp::asts::Ast::Stage2_GenTopLvlScopes(
    analyse::scopes::ScopeManager *sm,
    meta::CompilerMetaData *)
    -> void {
    _Scope = sm->CurrentScope;
}

auto spp::asts::Ast::GetAstCtx() const
    -> Ast* {
    return _Ctx;
}

auto spp::asts::Ast::GetAstScope() const
    -> analyse::scopes::Scope* {
    return _Scope;
}

auto spp::asts::Ast::SetAstCtx(
    Ast *ctx)
    -> void {
    _Ctx = ctx;
}

auto spp::asts::Ast::SetAstScope(
    analyse::scopes::Scope *scope)
    -> void {
    _Scope = scope;
}

SPP_MOD_END
