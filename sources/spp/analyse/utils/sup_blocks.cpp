module;
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.sup_blocks;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.scope_tree;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.self_type;
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.type_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import genex;
import std;

/// [CHECKED]
auto spp::analyse::utils::sup_blocks::IsSupBlock(
  Ast const *node) -> bool {
  // Simple multi-cast check.
  return node != nullptr and (
    node->To<SupPrototypeFunctionsAst>() != nullptr or
    node->To<SupPrototypeExtensionAst>() != nullptr);
}

/// [CHECKED]
auto spp::analyse::utils::sup_blocks::EnclosingSupBlock(
  Scope const &scope) -> Scope const* {
  // Get the ancestral scope whose node is a sup or sup-ext
  // block, and the name is compiler generated. This is wrapping
  // the overload set of function prototype asts.
  for (auto const *s = &scope; s != nullptr; s = s->Parent) {
    if (IsSupBlock(s->AstNode) and not AstName(s->AstNode)->IsCompilerGeneratedType()) { return s; }
  }
  return nullptr;
}

auto spp::analyse::utils::sup_blocks::CheckGnParams(
  GenericParameterGroupAst const &params, Vec<TypeAst const*> const &naming_types,
  ScopeManager const &sm) -> void {
  using errors::SppSuperimpositionOptionalGenericParameterError;
  using errors::SppSuperimpositionUnconstrainedGenericParameterError;
  const auto optional = params.GetOptionalParams();
  RaiseIf<SppSuperimpositionOptionalGenericParameterError>(
    not optional.empty(), {sm.CurrentScope}, ERR_ARGS(*optional[0]));
  if (naming_types.empty()) { return; }

  const auto unconstrained = params.GetAllParams()
    | genex::views::filter([&](auto const *param) {
      return not genex::any_of(naming_types, [&](auto const *type) {
        return type_predicates::DoesTypeNameAGnParam(*type, *param, *sm.CurrentScope);
      });
    })
    | genex::to<Vec>();
  RaiseIf<SppSuperimpositionUnconstrainedGenericParameterError>(
    not unconstrained.empty(), {sm.CurrentScope}, ERR_ARGS(*unconstrained[0]));
}

auto spp::analyse::utils::sup_blocks::LoadTarget(
  Ast const &block, Shared<TypeAst> &name, const bool files_nested_mock,
  ScopeManager &sm, meta::CompilerMetaData *meta) -> TypeSymbol* {
  {
    const auto _meta_guard = asts::meta::MetaGuard(meta);
    meta->AllowAbstractType = true;
    name->Stage7_AnalyseSemantics(&sm, meta);
  }
  RaiseIf<errors::SppSecondClassBorrowViolationError>(
    type_predicates::IsTypeBorrowed(*name, sm),
    {sm.CurrentScope}, ERR_ARGS(block, *name, "superimposition type"));

  // A "$" mock keeps its bare name: it is declared in the
  // module of the mock class it names.
  name = sm.CurrentScope->FindTypeSymbol(name.get())->FqName(true)->WithSourceSpanOf(*name);

  // A method's "$" mock block sits inside its "sup" block
  // rather than the module, and still reaches its mock class.
  const auto base_sym = sm.CurrentScope->FindHeadSymbol(*name);
  if (sm.CurrentScope->Parent == sm.CurrentScope->GetParentModule()
    or (files_nested_mock and name->IsCompilerGeneratedType())) {
    if (not base_sym->IsGn()) { sm.Tree().NormalSupBlocks[base_sym].emplace_back(sm.CurrentScope); }
    else { sm.Tree().GnSupBlocks.emplace_back(sm.CurrentScope); }
  }

  self_type::RegisterSelf(*name, sm);
  return base_sym;
}
