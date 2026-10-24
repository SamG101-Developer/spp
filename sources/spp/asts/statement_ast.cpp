module;
#include <spp/macros.hpp>

module spp.asts.statement_ast;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.type_ast;
import spp.asts.generate.common_types_precompiled;

SPP_MOD_BEGIN
StatementAst::StatementAst() = default;

StatementAst::~StatementAst() = default;

auto StatementAst::InferTypeRef(
  ScopeManager *sm, CompilerMetaData *) -> TypeRef {
  // All statements are inferred as the Void type.
  using generate::common_types_precompiled::VOID;
  return TypeRef::Of(*VOID, *sm->CurrentScope);
}

auto StatementAst::Diverges(
  ScopeManager *, CompilerMetaData *) -> bool {
  // By default, control continues past a statement.
  return false;
}

SPP_MOD_END
