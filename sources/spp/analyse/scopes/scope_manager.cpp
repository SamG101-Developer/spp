module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.scopes.scope_manager;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.monomorphization_utils;
import spp.analyse.utils.type_compare;
import spp.analyse.utils.type_members;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_ast;
import spp.asts.generic_parameter_type_inline_constraints_ast;
import spp.asts.identifier_ast;
import spp.asts.module_implementation_ast;
import spp.asts.module_member_ast;
import spp.asts.module_prototype_ast;
import spp.asts.sup_implementation_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.type_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.utils.error_formatter;
import genex;

namespace spp::analyse::scopes {
  namespace {
    auto GetSupGenericParamsFromScope(
      Scope const &sup_scope)
      -> asts::GenericParameterGroupAst const* {
      //
      using namespace spp::asts;
      if (auto const *fns = AstAs<SupPrototypeFunctionsAst>(sup_scope.AstNode); fns != nullptr) {
        return fns->GnParamGroup.get();
      }
      if (auto const *ext = AstAs<SupPrototypeExtensionAst>(sup_scope.AstNode); ext != nullptr) {
        return ext->GnParamGroup.get();
      }
      return nullptr;
    }

    auto SupConstrainsItsParams(
      Scope const &sup_scope)
      -> bool {
      //
      auto const *params = GetSupGenericParamsFromScope(sup_scope);
      if (params == nullptr) { return false; }

      return genex::any_of(params->GetTypeParams(), [](auto const *p) {
        return not p->Constraints->Constraints.IsEmpty();
      });
    }
  }
}

SPP_MOD_BEGIN
spp::analyse::scopes::ScopeManager::ScopeManager(
  Shared<Scope> const &global_scope,
  Scope *current_scope) :
  GlobalScope(global_scope),
  CurrentScope(current_scope ? current_scope : global_scope.get()) {
}

spp::analyse::scopes::ScopeManager::~ScopeManager() = default;

auto spp::analyse::scopes::ScopeManager::Iter() const
  -> ScopeRange {
  return ScopeRange{CurrentScope};
}

auto spp::analyse::scopes::ScopeManager::Reset(
  Scope *scope,
  std::optional<ScopeIterator> iterator)
  -> void {
  // Set the current scope to the provided scope or global scope.
  CurrentScope = scope ? scope : GlobalScope.get();
  _It = iterator.has_value() ? *iterator : ScopeIterator{CurrentScope};
}

auto spp::analyse::scopes::ScopeManager::CreateAndMoveIntoNewScope(
  ScopeName const &name,
  asts::Ast *ast,
  spp::utils::errors::ErrorFormatter *error_formatter)
  -> Scope* {
  // Create a new scope, using the current scope as the parent scope.
  auto scope = MakeUnique<Scope>(name, CurrentScope, ast, error_formatter);
  CurrentScope->Children.EmplaceBack(std::move(scope));
  ++_It;

  // Set the new scope as the current scope, and advance the iterator to match.
  CurrentScope = CurrentScope->Children.Back().get();
  return CurrentScope;
}

auto spp::analyse::scopes::ScopeManager::MoveOutOfCurrentScope()
  -> Scope* {
  // Exit the current scope into the parent scope.
  CurrentScope = CurrentScope->Parent;
  return CurrentScope;
}

auto spp::analyse::scopes::ScopeManager::MoveToNextScope(
  const bool ignore_alias_class_scopes)
  -> Scope* {
  // For debugging mode only, check if the iterator has reached the end of the generator.
  // Move to the next scope by advancing the iterator.
  CurrentScope = *++_It;
  while (ignore_alias_class_scopes and CurrentScope->TySym != nullptr
    and
    CurrentScope->TySym->Alias != nullptr) {
    CurrentScope = *++_It;
  }
  return CurrentScope;
}

auto spp::analyse::scopes::ScopeManager::ExhaustScope()
  -> void {
  // Manual scope skipping.
  const auto final_scope = CurrentScope->FinalChildScope();
  while (CurrentScope != final_scope) {
    MoveToNextScope(false);
  }
}

auto spp::analyse::scopes::ScopeManager::AttachAllSuperScopes(
  asts::meta::CompilerMetaData *meta)
  -> void {
  // Ensure the scope manager is at the global scope. Attach every super scope structurally, deferring any generic
  // constraint check. This is order-independent because it does not depend on other types' super scopes already being
  // attached. The issue before was that some generic constraints checks were failing deep down because constraints
  // themselves are modelled as sup-scopes. Note for future: just trust this comment.
  Reset();
  auto deferred = Vec<DeferredSupConstraint>();
  for (auto *scope : Iter()) {
    if (scope->TySym == nullptr) { continue; }
    AttachSpecificSuperScopes(*scope, meta, &deferred);
  }

  // Now that every type has its super scopes, validate the deferred generic constraints and prune any attachment
  // whose constraint is unsatisfied (eg "SliceMut[StrView, USize]" retains the constrained
  // "sup [V, I: Zero] SliceMut[V, I]" block only if USize satisfies "Zero").
  Reset();
  PruneUnsatisfiedSupConstraints(deferred, meta);
  Reset();
}

auto spp::analyse::scopes::ScopeManager::AttachSpecificSuperScopes(
  Scope &scope,
  asts::meta::CompilerMetaData *meta,
  Vec<DeferredSupConstraint> *deferred) const
  -> void {
  // Handle type symbols.
  if (scope.TySym == nullptr) { return; }
  const auto non_generic_sym = scope.GetTypeSymbol(scope.TySym->FqName()->WithoutGenerics().get());

  // Looked up rather than indexed: three quarters of the types reaching here have no sup block of their own, and
  // "operator[]" would insert an empty entry for each of them - tens of thousands of dead keys the rest of the
  // compilation then hashes past - as well as copying the list out on every call.
  const auto it = normal_sup_blocks.find(non_generic_sym);
  auto const *const normal = it != normal_sup_blocks.end() ? &it->second : nullptr;

  // A pure generic block ("sup [T] T") names no type, so it applies to every one of them and has to be merged in.
  // There are usually none at all, and then the stored list is handed over as it stands.
  if (generic_sup_blocks.IsEmpty()) {
    if (normal != nullptr) { AttachSpecificSuperScopesImpl(scope, *normal, meta, deferred); }
    return;
  }

  auto scopes = normal != nullptr ? *normal : Vec<Scope*>();
  scopes.AppendRange(generic_sup_blocks);
  AttachSpecificSuperScopesImpl(scope, scopes, meta, deferred);
}

auto spp::analyse::scopes::ScopeManager::AttachSpecificSuperScopesImpl(
  Scope &scope,
  Vec<Scope*> const &sup_scopes,
  asts::meta::CompilerMetaData *meta,
  Vec<DeferredSupConstraint> *deferred) const
  -> void {
  //
  using utils::monomorphization_utils::CreateGenericSupScope;
  using utils::type_compare::RelaxedTypeEq;
  using utils::type_compare::GenericInferenceMap;
  if (sup_scopes.IsEmpty()) { return; }

  // Clear the sup scopes list.
  BumpTypeStructureGeneration();
  scope.DirectSupScopes.Clear();
  const auto fq_type = scope.TySym->FqName();
  auto const &cls_sym = scope.TySym;

  // Iterate through all the super scopes and check if the name matches.
  for (auto *sup_scope : sup_scopes) {
    // Perform a relaxed comparison between the two types (allows for specializations to match bases).
    auto scope_generics_map = GenericInferenceMap();

    // Load the generics.
    if (not RelaxedTypeEq(
      *fq_type, *asts::AstName(sup_scope->AstNode), *scope.TySym->ScopeDefinedIn, *sup_scope,
      scope_generics_map, false, false, true)) { continue; }
    auto scope_generics = asts::GenericArgumentGroupAst::FromMap(std::move(scope_generics_map));

    // Create a generic version of the super scope if needed.
    auto new_sup_scope = static_cast<Scope*>(nullptr);
    auto new_cls_scope = static_cast<Scope*>(nullptr);
    auto sup_sym = static_cast<TypeSymbol*>(nullptr);
    auto defer_constraint = false;

    // Todo: Is this "if-else" quite correct? 2 conditions in the "if", then no "else if" block.
    if (not scope_generics->Args.IsEmpty()
      and not genex::contains(generic_sup_blocks, sup_scope)) {
      const auto external_generics = scope.TySym->ScopeDefinedIn->GetExtendedGenericSymbols(
        scope_generics->GetAllArgs());
      std::tie(new_sup_scope, new_cls_scope) = CreateGenericSupScope(
        *sup_scope, scope, *scope_generics, external_generics, this, meta);
      sup_sym = new_cls_scope ? new_cls_scope->TySym.get() : nullptr;

      // When deferring (the bulk pass), match structurally only and record the constraint for later; the
      // constrained type's own super scopes might not be attached yet, so an inline check would be non order-
      // agnostic. On-demand attachment (deferred == nullptr) checks the constraint inline as before.
      if (auto _ = GenericInferenceMap(); not RelaxedTypeEq(
        *fq_type, *asts::AstName(sup_scope->AstNode), *scope.TySym->ScopeDefinedIn, *new_sup_scope,
        _, false, deferred == nullptr, true)) { continue; }
      defer_constraint = deferred != nullptr;
    }
    else {
      const auto sup_proto = AstAs<asts::SupPrototypeExtensionAst>(sup_scope->AstNode);
      new_sup_scope = sup_scope;
      new_cls_scope = sup_proto ? scope.GetTypeSymbol(sup_proto->SuperClass.get())->LinkedScope : nullptr;
      sup_sym = new_cls_scope ? new_cls_scope->TySym.get() : nullptr;

      // Nothing bound, so there is no substitution to record - but a constraint declared here still has to be
      // checked. A variadic parameter is what reaches this: it stands for a list of types, so the match binds it to
      // nothing, while "sup [..T: Copy] Tup[T]" still constrains every element it swallowed. Deferred in the bulk
      // pass and checked inline on demand, for the same reasons as the branch above.
      if (SupConstrainsItsParams(*sup_scope)) {
        if (deferred != nullptr) { defer_constraint = true; }
        else if (auto _ = GenericInferenceMap(); not RelaxedTypeEq(
          *fq_type, *asts::AstName(sup_scope->AstNode), *scope.TySym->ScopeDefinedIn, *new_sup_scope,
          _, false, true, true)) { continue; }
      }
    }

    // Prevent double inheritance, cyclic inheritance and self extension.
    if (const auto ext_ast = AstAs<asts::SupPrototypeExtensionAst>(sup_scope->AstNode); ext_ast != nullptr) {
      ext_ast->CheckCyclicExtension(*sup_sym, *sup_scope);
      ext_ast->CheckDoubleExtension(*cls_sym, *sup_scope);
      ext_ast->CheckSelfExtension(*sup_scope);
    }

    // Register the super scope against the current scope.
    BumpTypeStructureGeneration();
    scope.DirectSupScopes.EmplaceBack(new_sup_scope);

    // Register the super scope's class scope against the current scope, if it is different. This "difference" check
    // ensures that "sup [T] T ext A" doesn't create a "sup A ext A" link.
    const auto cls_scope_attached = new_cls_scope and scope.TySym != new_cls_scope->TySym;
    if (cls_scope_attached) {
      // Todo: is this definitely the generically substituted "new_cls_scope"?
      BumpTypeStructureGeneration();
      scope.DirectSupScopes.EmplaceBack(new_cls_scope);
    }

    // Record the constrained attachment so its generic constraint can be validated (and the attachment pruned
    // if unsatisfied) once every type has its super scopes.
    if (defer_constraint) {
      deferred->EmplaceBack(DeferredSupConstraint{
        &scope, new_sup_scope, cls_scope_attached ? new_cls_scope : nullptr, sup_scope
      });
    }

    // Check for conflicting "cmp" or "type" statements in the super scopes.
    if (AstAs<asts::SupPrototypeExtensionAst>(sup_scope->AstNode) or
      AstAs<asts::SupPrototypeFunctionsAst>(sup_scope->AstNode)) {
      CheckConflictingTypeOrCmpStatements(*cls_sym, *sup_scope);
    }
  }
}

auto spp::analyse::scopes::ScopeManager::PruneUnsatisfiedSupConstraints(
  Vec<DeferredSupConstraint> &deferred,
  asts::meta::CompilerMetaData * /*meta*/) const
  -> void {
  // Todo: Genex usage
  using utils::type_compare::RelaxedTypeEq;
  using utils::type_compare::GenericInferenceMap;

  // Repeat until no further attachments are pruned: pruning one attachment can invalidate the constraint of
  // another that depends on it (transitive constraint chains), so a single pass is not sufficient.
  auto changed = true;
  while (changed) {
    changed = false;
    for (auto &dc : deferred) {
      // Skip records that have already been pruned.
      if (dc.owner_scope == nullptr) { continue; }

      // Re-run the constraint check that was deferred previously (RelaxedTypeEq with constraint checking
      // enabled), now against the complete super scope graph.
      auto _ = GenericInferenceMap();
      const auto fq_type = dc.owner_scope->TySym->FqName();
      if (RelaxedTypeEq(
        *fq_type, *asts::AstName(dc.base_sup_scope->AstNode), *dc.owner_scope->TySym->ScopeDefinedIn, *dc.sup_scope,
        _, false, true, true)) { continue; }

      // The constraint is not satisfied, so remove the attached super scope (and its paired class scope).
      auto &sup_scopes = dc.owner_scope->DirectSupScopes;
      sup_scopes |= genex::actions::remove_if([&](auto const *s) {
        return s == dc.sup_scope or (dc.sup_cls_scope != nullptr and s == dc.sup_cls_scope);
      });
      dc.owner_scope = nullptr;
      changed = true;
    }
  }
}

auto spp::analyse::scopes::ScopeManager::CheckConflictingTypeOrCmpStatements(
  TypeSymbol const &cls_sym,
  Scope const &sup_scope)
  -> void {
  // Get the scopes to check for conflicts in.
  auto dummy = utils::type_compare::GenericInferenceMap();
  const auto existing_scopes = cls_sym.LinkedScope->DirectSupScopes
    | genex::views::filter([&](auto *scope) {
      return AstAs<asts::SupPrototypeExtensionAst>(scope->AstNode)
        or AstAs<asts::SupPrototypeFunctionsAst>(scope->AstNode);
    })
    | genex::views::filter([&](auto *scope) {
      return utils::type_compare::RelaxedTypeEq(
        *asts::AstName(sup_scope.AstNode), *asts::AstName(scope->AstNode), sup_scope, *scope->AstNode->GetAstScope(),
        dummy);
    })
    | genex::to<Vec>();

  // Check for conflicting "type" statements.
  Vec<Shared<asts::TypeIdentifierAst>> new_types;
  for (auto const *scope : existing_scopes) {
    const auto body = asts::AstBody(scope->AstNode);
    for (auto const *member : body) {
      if (auto const *type_stmt = member->To<asts::TypeStatementAst>(); type_stmt != nullptr) {
        for (auto const &new_type : new_types) {
          RaiseIf<errors::SppIdentifierDuplicateError>(
            *new_type == *type_stmt->NewType, {scope, &sup_scope},
            ERR_ARGS(*new_type, *type_stmt->NewType, "associated type"));
        }
        new_types.EmplaceBack(type_stmt->NewType);
      }
    }
  }

  // Check for conflicting "cmp" statements.
  Vec<Shared<asts::IdentifierAst>> new_cmps;
  for (const auto *scope : existing_scopes) {
    const auto body = asts::AstBody(scope->AstNode);
    for (auto const *member : body) {
      if (auto const *cmp_stmt = member->To<asts::CmpStatementAst>(); cmp_stmt != nullptr and not cmp_stmt->Type->
        IsCompilerGeneratedType()) {
        for (auto const &new_cmp : new_cmps) {
          RaiseIf<errors::SppIdentifierDuplicateError>(
            *new_cmp == *cmp_stmt->Name, {scope, &sup_scope},
            ERR_ARGS(*new_cmp, *cmp_stmt->Name, "comptime constant"));
        }
        new_cmps.EmplaceBack(cmp_stmt->Name);
      }
    }
  }
}

auto spp::analyse::scopes::ScopeManager::CurrentIterator()
  -> ScopeIterator& {
  return _It;
}

auto spp::analyse::scopes::ScopeManager::SelfProto() const
  -> asts::ClassPrototypeAst* {
  return _SelfProto.get();
}

auto spp::analyse::scopes::ScopeManager::Cleanup() -> void {
  normal_sup_blocks.clear();
  utils::type_members::ClearUnimplementedAbstractMethodsCache();
  utils::monomorphization_utils::ClearSupScopeInstantiations();
  generic_sup_blocks.Clear();
  temp_scopes.Clear();
  asts::GenericParameterTypeAst::ClearDummyScopes();
}

SPP_MOD_END
