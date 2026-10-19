module;
#include <spp/macros.hpp>

module spp.asts.mixins.type_inferrable_ast;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.asts.ast;
import spp.asts.type_ast;

SPP_MOD_BEGIN
TypeInferrableAst::TypeInferrableAst() = default;
TypeInferrableAst::~TypeInferrableAst() = default;

auto TypeInferrableAst::InferType(
  ScopeManager *sm, CompilerMetaData *meta) -> Shared<TypeAst> {
  // The resolved type's name, placed at this expression rather
  // than where its class is declared, so a message pointing at
  // the type points here.
  const auto type = InferTypeRef(sm, meta).AstIn(*sm->CurrentScope);
  const auto site = dynamic_cast<Ast const*>(this);
  return type != nullptr and site != nullptr
    ? type->WithSourceSpanAt(*site)
    : type;
}

SPP_MOD_END
