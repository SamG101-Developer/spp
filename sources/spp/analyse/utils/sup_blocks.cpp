module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.sup_blocks;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.type_predicates;
import spp.analyse.utils.type_resolution;
import spp.asts.ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import genex;
import std;

auto spp::analyse::utils::sup_blocks::CheckGnParams(
  GenericParameterGroupAst const &params,
  Vec<TypeAst const*> const &naming_types,
  ScopeManager const &sm)
  -> void {
  using errors::SppSuperimpositionOptionalGenericParameterError;
  using errors::SppSuperimpositionUnconstrainedGenericParameterError;
  const auto optional = params.GetOptionalParams();
  RaiseIf<SppSuperimpositionOptionalGenericParameterError>(
    not optional.IsEmpty(), {sm.CurrentScope}, ERR_ARGS(*optional[0]));
  if (naming_types.IsEmpty()) { return; }

  const auto unconstrained = params.GetAllParams()
    | genex::views::filter([&](auto const *param) {
      return not genex::any_of(naming_types, [&](auto const *type) {
        return type_resolution::DoesTypeNameAGnParam(*type, *param, *sm.CurrentScope);
      });
    })
    | genex::to<Vec>();
  RaiseIf<SppSuperimpositionUnconstrainedGenericParameterError>(
    not unconstrained.IsEmpty(), {sm.CurrentScope}, ERR_ARGS(*unconstrained[0]));
}

auto spp::analyse::utils::sup_blocks::RegisterSelf(
  TypeAst const &name,
  ScopeManager &sm)
  -> void {
  if (name.IsCompilerGeneratedType()) { return; }

  // A block over an alias ("sup U8") is over what the alias stands for: its "Self" is the target, not the class at
  // the end of the alias's chain (which only links the template).
  auto *sym = sm.CurrentScope->FindTypeSymbol(&name);
  if (sym->Alias != nullptr) { sym = sym->AliasTarget(*sm.CurrentScope); }
  sm.AddSelfTypeSymbol(sym->LinkedScope, name.PosStart());
}

auto spp::analyse::utils::sup_blocks::RegisterProvisionalSelf(
  TypeAst const &name,
  ScopeManager &sm)
  -> void {
  if (name.IsCompilerGeneratedType()) { return; }
  if (const auto base_sym = sm.CurrentScope->FindHeadSymbol(name)) {
    sm.AddSelfTypeSymbol(base_sym->LinkedScope, name.PosStart());
  }
}

auto spp::analyse::utils::sup_blocks::LoadTarget(
  Ast const &block,
  Shared<TypeAst> &name,
  const bool files_nested_mock,
  ScopeManager &sm,
  meta::CompilerMetaData *meta)
  -> TypeSymbol* {
  {
    const auto _meta_guard = asts::meta::MetaGuard(meta);
    meta->AllowAbstractType = true;
    name->Stage7_AnalyseSemantics(&sm, meta);
  }
  RaiseIf<errors::SppSecondClassBorrowViolationError>(
    type_predicates::IsTypeBorrowed(*name, sm),
    {sm.CurrentScope}, ERR_ARGS(block, *name, "superimposition type"));

  // A "$" mock keeps its bare name: it is declared in the module of the mock class it names.
  name = sm.CurrentScope->FindTypeSymbol(name.get())->FqName(true)->WithSourceSpanOf(*name);

  // A method's "$" mock block sits inside its "sup" block rather than the module, and still reaches its mock class.
  const auto base_sym = sm.CurrentScope->FindHeadSymbol(*name);
  if (sm.CurrentScope->Parent == sm.CurrentScope->GetParentModule()
    or (files_nested_mock and name->IsCompilerGeneratedType())) {
    if (not base_sym->IsGn()) { ScopeManager::NormalSupBlocks[base_sym].EmplaceBack(sm.CurrentScope); }
    else { ScopeManager::GnSupBlocks.EmplaceBack(sm.CurrentScope); }
  }

  RegisterSelf(*name, sm);
  return base_sym;
}
