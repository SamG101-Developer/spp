module;
#include <spp/macros.hpp>

module spp.asts.mixins.type_inferrable_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.type_ast;

SPP_MOD_BEGIN
TypeInferrableAst::TypeInferrableAst() = default;
TypeInferrableAst::~TypeInferrableAst() = default;

auto TypeInferrableAst::InferTypeForDisplay(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> Shared<TypeAst> {
  // Default behaviour is to use the normal inference steps.
  return InferType(sm, meta);
}

auto TypeInferrableAst::InferTypeRef(
  ScopeManager *sm,
  CompilerMetaData *meta)
  -> TypeRef {
  const auto type = InferType(sm, meta);
  return type != nullptr ? TypeRef::Of(*type, *sm->CurrentScope) : TypeRef{};
}

SPP_MOD_END
