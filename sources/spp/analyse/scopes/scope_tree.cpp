module;
#include <spp/macros.hpp>

module spp.analyse.scopes.scope_tree;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.monomorphization;
import spp.analyse.utils.sup_attachment;
import spp.analyse.utils.type_members;
import spp.asts.closure_expression_ast;
import spp.asts.generic_parameter_ast;

SPP_MOD_BEGIN
ScopeTree::ScopeTree(
  Shared<Scope> global) :
  Global(std::move(global)) {
  Global->OwningTree = this;
}

ScopeTree::~ScopeTree() {
  // The caches elsewhere hold this compile's scopes, so they are
  // cleared while the scopes are still alive, before the members
  // (the scopes themselves) are freed.
  spp::analyse::utils::monomorphization::StopInstantiatingOnRead();
  Scope::ClearOpaqueCompValues();
  spp::analyse::scopes::ClearGnParams();
  spp::analyse::utils::sup_attachment::ClearSupStatementsCache();
  spp::analyse::utils::type_members::ClearUnimplementedAbstractMethodsCache();
  spp::asts::GenericParameterAst::ClearDummyScopes();
  spp::asts::ClosureExpressionAst::ClearMockAsts();
}

SPP_MOD_END
