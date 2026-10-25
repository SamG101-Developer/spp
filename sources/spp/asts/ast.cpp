module;
#include <spp/macros.hpp>

module spp.asts.ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.utils.ice;
import std;

SPP_MOD_BEGIN
Ast::Ast() = default;

Ast::~Ast() = default;

auto Ast::_AsFailed(
  Ast const *self,
  const std::source_location raised_at)
  -> void {
  // Quote the node when there is one: its kind is what the
  // casting site got wrong.
  if (self == nullptr) { Ice("a null ast cast as a kind it must be", raised_at); }
  Ice(*self, "an ast cast as a kind it is not", raised_at);
}

auto Ast::Size() const -> std::size_t {
  // The size is the difference between the two positions.
  return PosEnd() - PosStart();
}

auto Ast::AnkerlHash() const -> std::size_t {
  // Default hash is empty (0).
  return 0uz;
}

auto Ast::Stage1_PreProcess(
  Ast *ctx) -> void {
  // Bind the context by default.
  _Ctx = ctx;
}

auto Ast::Stage2_GenTopLvlScopes(
  ScopeManager *sm, CompilerMetaData *) -> void {
  // Bind the scope by default.
  _Scope = sm->CurrentScope;
}

auto Ast::GetAstCtx() const -> Ast* {
  // Context getter.
  return _Ctx;
}

auto Ast::GetAstScope() const -> Scope* {
  // Scope getter.
  return _Scope;
}

auto Ast::SetAstCtx(
  Ast *ctx) -> void {
  // Context setter.
  _Ctx = ctx;
}

auto Ast::SetAstScope(
  Scope *scope) -> void {
  // Scope setter.
  _Scope = scope;
}

auto Ast::IsAllowedInDefault() const -> bool {
  // Default implementation is to prevent it.
  return false;
}

SPP_MOD_END
