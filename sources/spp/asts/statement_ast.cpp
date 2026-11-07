module;
#include <spp/macros.hpp>

module spp.asts.statement_ast;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.type_ast;
import spp.asts.generate.common_types_precompiled;

use_alias(spp::analyse::scopes, ScopeManager);
use_alias(spp::analyse::scopes, TypeRef);
use_alias(spp::asts::meta, CompilerMetaData);
use_alias(spp::codegen, LlvmCtx);

SPP_MOD_BEGIN
spp::asts::StatementAst::StatementAst() = default;

spp::asts::StatementAst::~StatementAst() = default;

auto spp::asts::StatementAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *) -> TypeRef {
  // All statements are inferred as the Void type.
  using generate::common_types_precompiled::VOID;
  return TypeRef::Of(*VOID, *sm->CurrentScope);
}

auto spp::asts::StatementAst::Diverges(
  ScopeManager *, CompilerMetaData *) -> bool {
  // By default, control continues past a statement.
  return false;
}

SPP_MOD_END
