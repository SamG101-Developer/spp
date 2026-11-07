module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.asts.ast;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.utils.ice;
import spp.utils.stats;
import std;

use_alias(spp::analyse::scopes, Scope);
use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::Ast::Ast() {
  spp::utils::stats::Bump(spp::utils::stats::Counter::AstNodesCreated);
}

spp::asts::Ast::~Ast() = default;

auto spp::asts::Ast::_AsFailed(
  Ast const *self,
  const std::source_location raised_at)
  -> void {
  // Quote the node when there is one: its kind is what the
  // casting site got wrong.
  if (self == nullptr) { Ice("a null ast cast as a kind it must be", raised_at); }
  Ice(*self, "an ast cast as a kind it is not", raised_at);
}

auto spp::asts::Ast::Size() const -> std::size_t {
  // The size is the difference between the two positions.
  return PosEnd() - PosStart();
}

auto spp::asts::Ast::AnkerlHash() const -> std::size_t {
  // Default hash is empty (0).
  return 0uz;
}

auto spp::asts::Ast::Stage1_PreProcess(
  Ast *ctx) -> void {
  // Bind the context by default.
  _Ctx = ctx;
}

auto spp::asts::Ast::Stage2_GenTopLvlScopes(
  ScopeManager *sm, CompilerMetaData *) -> void {
  // Bind the scope by default.
  _Scope = sm->CurrentScope;
}

auto spp::asts::Ast::GetAstCtx() const -> Ast* {
  // Context getter.
  return _Ctx;
}

auto spp::asts::Ast::GetAstScope() const -> Scope* {
  // Scope getter.
  return _Scope;
}

auto spp::asts::Ast::SetAstCtx(
  Ast *ctx) -> void {
  // Context setter.
  _Ctx = ctx;
}

auto spp::asts::Ast::SetAstScope(
  Scope *scope) -> void {
  // Scope setter.
  _Scope = scope;
}

auto spp::asts::Ast::IsAllowedInDefault() const -> bool {
  // Default implementation is to prevent it.
  return false;
}

auto spp::asts::Ast::EvaluateCompTime(
  ScopeManager *, CompilerMetaData *) -> void {
  // Default behaviour: this AST does not support
  // comptime resolution, so throw an error.
  using namespace spp::analyse::errors;
  Raise<SppInvalidCompTimeOperationError>(ERR_ARGS(*this));
}

SPP_MOD_END
