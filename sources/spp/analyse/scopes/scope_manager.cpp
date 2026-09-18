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
import spp.analyse.utils.type_predicates;
import spp.asts.ast;
import spp.asts.class_prototype_ast;
import spp.asts.closure_expression_ast;
import spp.asts.cmp_statement_ast;
import spp.asts.function_prototype_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_group_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
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
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import spp.codegen.llvm_type;
import spp.utils.error_formatter;
import genex;
import std;

namespace spp::analyse::scopes {
  namespace {
    /// Universal generic parameter grabber from "sup" and "sup-ext"
    /// blocks. The returning type is the same so easy to unify.
    auto GetSupGenericParamsFromScope(
      Scope const &sup_scope) -> GenericParameterGroupAst const* {
      using namespace spp::asts;

      // Extract from raw "sup" blocks.
      if (const auto fns = AstAs<SupPrototypeFunctionsAst>(sup_scope.AstNode); fns != nullptr) {
        return fns->GnParamGroup.get();
      }

      // Extract from "sup ext" blocks.
      if (const auto ext = AstAs<SupPrototypeExtensionAst>(sup_scope.AstNode); ext != nullptr) {
        return ext->GnParamGroup.get();
      }
      return nullptr;
    }

    /// Types in sup-matches are already constrained to the same
    /// base-type, to save on massive amounts of type-matching;
    /// that's the "coarse" check. The refined check includes
    /// generics and constraints.
    auto SupPatternApplies(
      TypeSymbol const &type_sym, TypeAst const &type, Scope const &type_scope,
      TypeAst const &pattern, Scope const &pattern_scope,
      utils::type_compare::GenericInferenceMap &generics,
      const bool check_constraints) -> bool {
      // Double-sided relaxed type equality.
      using utils::type_compare::RelaxedTypeEq;
      return type_sym.IsTypeGeneric()
        ? RelaxedTypeEq(pattern, type, pattern_scope, type_scope, generics, false, check_constraints)
        : RelaxedTypeEq(type, pattern, type_scope, pattern_scope, generics, false, check_constraints);
    }

    /// Helper to detect if a sup block declares a variadic generic,
    /// or a generic "pack". Check the group for a variadic "..T"
    /// generic parameter.
    auto SupDeclaresAPack(
      Scope const &sup_scope)
      -> bool {
      // Get all generics and check for a variadic.
      const auto params = GetSupGenericParamsFromScope(sup_scope);
      return params != nullptr and params->GetVariadicParams() != nullptr;
    }

    /// Check if any of the generics on a sup block contain
    /// constraints, which open up an additional check. If this is
    /// false, then an extra (expensive) check is avoided, which
    /// is why we bother checking in the first place.
    auto SupConstrainsItsParams(
      Scope const &sup_scope) -> bool {
      // Collect the generics on the sup block.
      auto const *params = GetSupGenericParamsFromScope(sup_scope);
      if (params == nullptr) { return false; }

      // Check if any of the generics have constraints.
      return genex::any_of(params->GetTypeParams(), [](auto const *p) {
        return not p->Constraints->Constraints.IsEmpty();
      });
    }
  }
}

SPP_MOD_BEGIN
ScopeManager::ScopeManager(
  Shared<Scope> const &global_scope,
  Scope *current_scope) :
  GlobalScope(global_scope),
  CurrentScope(current_scope ? current_scope : global_scope.get()) {
}

ScopeManager::~ScopeManager() = default;

auto ScopeManager::Iter() const -> ScopeRange {
  // Create an iterator for the scope.
  return ScopeRange{CurrentScope};
}

auto ScopeManager::Reset(
  Scope *scope, std::optional<ScopeIterator> iterator) -> void {
  // Set the current scope to the provided scope or global
  // scope.
  CurrentScope = scope ? scope : GlobalScope.get();
  _It = iterator.has_value() ? *iterator : ScopeIterator{CurrentScope};
}

auto ScopeManager::CreateAndMoveIntoNewScope(
  ScopeName const &name, Ast *ast, ErrorFormatter *error_formatter)
  -> Scope* {
  // Create a new scope, using the current scope as the parent
  // scope.
  auto scope = MakeUnique<Scope>(name, CurrentScope, ast, error_formatter);
  CurrentScope->Children.EmplaceBack(std::move(scope));
  ++_It;

  // Set the new scope as the current scope, and advance the
  // iterator to match.
  CurrentScope = CurrentScope->Children.Back().get();
  return CurrentScope;
}

auto ScopeManager::MoveOutOfCurrentScope() -> Scope* {
  // Exit the current scope into the parent scope.
  CurrentScope = CurrentScope->Parent;
  return CurrentScope;
}

auto ScopeManager::MoveToNextScope(
  const bool ignore_alias_class_scopes) -> Scope* {
  // For debugging mode only, check if the iterator has reached
  // the end of the generator. Move to the next scope by advancing
  // the iterator.
  CurrentScope = *++_It;
  while (ignore_alias_class_scopes and CurrentScope->TySym != nullptr
    and CurrentScope->TySym->Alias != nullptr) {
    CurrentScope = *++_It;
  }
  return CurrentScope;
}

auto ScopeManager::ExhaustScope() -> void {
  // Manual scope skipping.
  const auto final_scope = CurrentScope->FinalChildScope();
  while (CurrentScope != final_scope) {
    MoveToNextScope(false);
  }
}

auto ScopeManager::AttachAllSuperScopes(
  CompilerMetaData *meta) -> void {
  // Attach every type's super scopes, checking each generic
  // constraint as it is attached. A constraint is checked
  // against the constrained type's own super scopes, which
  // critically may not be attached yet - so while this runs,
  // reading a scope's super scopes attaches them first, and
  // no check reads a half-built graph.
  struct HookReset {
    ~HookReset() { Scope::OnSupScopesRead = nullptr; }
  } const _hook_reset;

  // The recursive analysis technique. This avoids the double
  // sweep of attach all (ignoring constraints), and then
  // check the deferred constraints and prune.
  Scope::OnSupScopesRead = [this, meta](Scope const &read) {
    static_cast<void>(AttachSpecificSuperScopes(const_cast<Scope&>(read), meta));
  };

  // The sweep creates scopes as it runs - attaching a
  // constrained sup instantiates the substituted type - so
  // it repeats until a pass attaches nothing new.
  for (auto found_new = true; found_new;) {
    found_new = false;
    Reset();
    for (auto *scope : Iter()) {
      if (AttachSpecificSuperScopes(*scope, meta)) { found_new = true; }
    }
  }
  Reset();
}

auto ScopeManager::AttachSpecificSuperScopes(
  Scope &scope, CompilerMetaData *meta) const -> bool {
  using utils::type_predicates::TemplateOf;
  // Handle type symbols, each once. Marked first, so a scope
  // reached again while its own attachment runs (a cycle) is
  // not attached twice.
  if (scope.TySym == nullptr or scope.SupsAttached) { return false; }
  if (scope.TySym->Kind == TypeKind::GenericParam) { return false; }
  scope.SupsAttached = true;
  const auto non_generic_sym = TemplateOf(*scope.TySym, scope);

  // Get the sup blocks for the type if there are any.
  const auto it = normal_sup_blocks.find(non_generic_sym);
  const auto normal = it != normal_sup_blocks.end()
    ? &it->second
    : nullptr;

  // A pure generic block ("sup [T] T") names no type, so it
  // applies to every one of them and has to be merged in.
  // There are usually none at all, and then the stored list
  // is handed over as it stands.
  if (generic_sup_blocks.IsEmpty()) {
    if (normal != nullptr) { AttachSpecificSuperScopesImpl(scope, *normal, meta); }
  }
  else {
    auto scopes = normal != nullptr ? *normal : Vec<Scope*>();
    scopes.AppendRange(generic_sup_blocks);
    AttachSpecificSuperScopesImpl(scope, scopes, meta);
  }

  // Merge the overloads that appear in different sup-blocks
  // into one (shared) $MockType.
  CoalesceMethodMock(scope);
  return true;
}

auto ScopeManager::CoalesceMethodMock(
  Scope &scope) const
  -> void {
  // Sanity guard on symbol type; we only want to hit $MockTypes
  // here, and do an ast check too.
  if (scope.TySym == nullptr or scope.TySym->Kind != TypeKind::FunctionMock or scope.Parent == nullptr) { return; }
  const auto sup_node = scope.Parent->AstNode;
  if (AstAs<SupPrototypeFunctionsAst>(sup_node) == nullptr) { return; }
  if (AstAs<SupPrototypeExtensionAst>(sup_node) == nullptr) { return; }

  // Get the symbol for the type name being superimposed over, and
  // all the sup-blocks for that type. There might be none, in
  // which case, return early too.
  const auto owner_sym = scope.Parent->GetTypeSymbol(
    AstName(sup_node)->WithoutGenerics().get());
  const auto owner_blocks = owner_sym != nullptr
    ? normal_sup_blocks.find(owner_sym)
    : normal_sup_blocks.end();
  if (owner_blocks == normal_sup_blocks.end()) { return; }

  // Get the mock name, like $MockType for "fun mock_type()"
  // overloads, and begin iterating the matching sup blocks.
  const auto mock_name = scope.TySym->Name->WithoutGenerics();
  for (const auto block : owner_blocks->second) {
    if (block == scope.Parent) { continue; }
    const auto sibling = block->GetTypeSymbol(mock_name.get(), true, false);
    if (sibling == nullptr or sibling == scope.TySym.get()) { continue; }
    const auto sibling_blocks = normal_sup_blocks.find(sibling);
    if (sibling_blocks == normal_sup_blocks.end()) { continue; }

    for (auto *ext_scope : sibling_blocks->second) {
      const auto ext = AstAs<SupPrototypeExtensionAst>(ext_scope->AstNode);
      if (ext == nullptr or genex::contains(scope.DirectSupScopes, ext_scope)) { continue; }
      BumpTypeStructureGeneration();
      scope.DirectSupScopes.EmplaceBack(ext_scope);
      if (const auto fn_sym = ext_scope->GetTypeSymbol(ext->SuperClass.get());
        fn_sym != nullptr and fn_sym->LinkedScope != nullptr
        and not genex::contains(scope.DirectSupScopes, fn_sym->LinkedScope)) {
        scope.DirectSupScopes.EmplaceBack(fn_sym->LinkedScope);
      }
    }
  }
}

auto ScopeManager::AttachSpecificSuperScopesImpl(
  Scope &scope, Vec<Scope*> const &sup_scopes,
  CompilerMetaData *meta) const -> void {
  using utils::monomorphization_utils::CreateGenericSupScope;
  using utils::type_compare::RelaxedTypeEq;
  using utils::type_compare::GenericInferenceMap;
  if (sup_scopes.IsEmpty()) { return; }

  // Clear the sup scopes list.
  BumpTypeStructureGeneration();
  scope.DirectSupScopes.Clear();

  // Matched as a pattern: a template as itself over its own
  // parameters. For example, from Vec, get Vec[T=T]. Grab
  // other metadata.
  const auto fq_type = scope.TySym->GenericSelfName();
  auto const &cls_sym = scope.TySym;
  const auto is_mock = scope.TySym->IsMock();

  // Iterate through all the super scopes and check if the name
  // matches.
  for (const auto sup_scope : sup_scopes) {
    // Perform a relaxed comparison between the two types (allows
    // for specializations to match bases).
    auto scope_generics_map = GenericInferenceMap();
    const auto own_mock_block = is_mock and not genex::contains(generic_sup_blocks, sup_scope);

    // Load the generics into the "scope_generic_map" whilst
    // checking that these types match in a relaxed manner, via
    // generics and possibly. Disable constraint checking here.
    if (not own_mock_block and not SupPatternApplies(
      *scope.TySym, *fq_type, *scope.TySym->ScopeDefinedIn, *AstName(sup_scope->AstNode), *sup_scope,
      scope_generics_map, false)) { continue; }
    auto scope_generics = GenericArgumentGroupAst::FromMap(std::move(scope_generics_map));

    // Create a generic version of the super scope if needed.
    auto new_sup_scope = static_cast<Scope*>(nullptr);
    auto new_cls_scope = static_cast<Scope*>(nullptr);
    auto sup_sym = static_cast<TypeSymbol*>(nullptr);

    if ((not scope_generics->Args.IsEmpty()
        or (SupDeclaresAPack(*sup_scope) and not AstName(sup_scope->AstNode)->IsCompilerGeneratedType()))
      and not genex::contains(generic_sup_blocks, sup_scope)) {
      // Build the generic sup scope for this instantiation,
      // and get the new cls symbol.
      std::tie(new_sup_scope, new_cls_scope) = CreateGenericSupScope(
        *sup_scope, scope, *scope_generics, this, meta);
      sup_sym = new_cls_scope ? new_cls_scope->TySym.get() : nullptr;

      // The constraint is checked here, against the constrained
      // type's super scopes - attached first if they are not yet.
      if (auto _ = GenericInferenceMap(); not SupPatternApplies(
        *scope.TySym, *fq_type, *scope.TySym->ScopeDefinedIn,
        *AstName(sup_scope->AstNode), *new_sup_scope,
        _, true)) { continue; }
    }
    else {
      const auto sup_proto = AstAs<SupPrototypeExtensionAst>(
        sup_scope->AstNode);
      new_sup_scope = sup_scope;
      const auto sup_cls_sym = sup_proto
        ? sup_scope->GetTypeSymbol(sup_proto->SuperClass.get())
        : nullptr;
      new_cls_scope = sup_cls_sym ? sup_cls_sym->LinkedScope : nullptr;
      sup_sym = new_cls_scope ? new_cls_scope->TySym.get() : nullptr;

      // Nothing bound, so there is no substitution to record -
      // but a constraint declared here still has to be checked.
      // A variadic parameter is what reaches this: it stands
      // for a list of types, so the match binds it to nothing,
      // while "sup [..T: Copy] Tup[T]" still constrains every
      // element it swallowed.
      if (auto _ = GenericInferenceMap(); SupConstrainsItsParams(*sup_scope) and not SupPatternApplies(
        *scope.TySym, *fq_type, *scope.TySym->ScopeDefinedIn, *AstName(sup_scope->AstNode), *new_sup_scope,
        _, true)) { continue; }
    }

    // Prevent double inheritance, cyclic inheritance and self
    // extension.
    if (const auto ext_ast = AstAs<SupPrototypeExtensionAst>(sup_scope->AstNode);
      ext_ast != nullptr and not ext_ast->Name->IsCompilerGeneratedType()) {
      ext_ast->CheckCyclicExtension(*sup_sym, *sup_scope);
      ext_ast->CheckDoubleExtension(*cls_sym, *sup_scope);
      ext_ast->CheckSelfExtension(*sup_scope);
    }

    // Register the super scope against the current scope.
    BumpTypeStructureGeneration();
    scope.DirectSupScopes.EmplaceBack(new_sup_scope);

    // Register the super scope's class scope against the current
    // scope, if it is different. This "difference" check ensures
    // that "sup [T] T ext A" doesn't create a "sup A ext A" link.
    const auto cls_scope_attached = new_cls_scope and scope.TySym != new_cls_scope->TySym;
    if (cls_scope_attached) {
      BumpTypeStructureGeneration();
      scope.DirectSupScopes.EmplaceBack(new_cls_scope);
    }

    // Check for conflicting "cmp" or "type" statements in the super
    // scopes.
    if (AstAs<SupPrototypeExtensionAst>(sup_scope->AstNode) or
      AstAs<SupPrototypeFunctionsAst>(sup_scope->AstNode)) {
      CheckConflictingTypeOrCmpStatements(*cls_sym, *sup_scope);
    }
  }
}

namespace spp::analyse::scopes {
  /// Helpers for detecting "cmp" or "type" statement conflicts
  /// during superimposition.
  namespace {
    /// The "type" and "cmp" statements a "sup" scope declares,
    /// extracted once.
    struct SupStatements {
      Vec<TypeStatementAst const*> Types;
      Vec<CmpStatementAst const*> Cmps;
    };

    /// Cache sup statement group against the sup scope they
    /// were found in as an optimisation.
    auto SupStatementsCache() -> Map<Scope const*, SupStatements>& {
      static auto cache = Map<Scope const*, SupStatements>();
      return cache;
    }

    /// Extract the sup statements of a sup block. This doesn't
    /// do any conflict checking, just extraction.
    auto SupStatementsOf(Scope const *scope) -> SupStatements const& {
      // Get the cache, and use it if there is an entry for the
      // requested scope.
      auto &cache = SupStatementsCache();
      if (const auto hit = cache.find(scope); hit != cache.end()) { return hit->second; }

      // Otherwise, we need to generate the mapping and inject it
      // into the cache.
      auto found = SupStatements();
      for (const auto member : AstBody(scope->AstNode)) {
        if (const auto type_stmt = member->To<TypeStatementAst>();
          type_stmt != nullptr) {
          found.Types.EmplaceBack(type_stmt);
        }
        else if (const auto cmp_stmt = member->To<CmpStatementAst>();
          cmp_stmt != nullptr and not cmp_stmt->Type->IsCompilerGeneratedType()) {
          found.Cmps.EmplaceBack(cmp_stmt);
        }
      }

      // Cache and return the mapping.
      return cache.emplace(scope, std::move(found)).first->second;
    }
  }
}

auto ScopeManager::CheckConflictingTypeOrCmpStatements(
  TypeSymbol const &cls_sym,
  Scope const &sup_scope)
  -> void {
  // A "$" mock owns nothing that can conflict. It provides
  // overload-coalescing mock types.
  if (cls_sym.IsMock()) { return; }

  // Get the scopes to check for conflicts in.
  auto dummy = utils::type_compare::GenericInferenceMap();

  // The name will be the same for every block being considered.
  const auto mine_name = AstName(sup_scope.AstNode);
  const auto existing_scopes = cls_sym.LinkedScope->DirectSupScopes
    | genex::views::filter([&](auto *scope) {
      return AstAs<SupPrototypeExtensionAst>(scope->AstNode)
        or AstAs<SupPrototypeFunctionsAst>(scope->AstNode);
    })
    | genex::views::filter([&](auto *scope) {
      const auto theirs_name = AstName(scope->AstNode);
      auto const &theirs_scope = *scope->AstNode->GetAstScope();
      return utils::type_compare::RelaxedTypeEq(*mine_name, *theirs_name, sup_scope, theirs_scope, dummy)
        or utils::type_compare::RelaxedTypeEq(*theirs_name, *mine_name, theirs_scope, sup_scope, dummy);
    })
    | genex::to<Vec>();

  // Check for conflicting "type" statements. Each name keeps the
  // scope it was written in, so the first one is reported from
  // its own file.
  Vec<Pair<Shared<TypeIdentifierAst>, Scope const*>> new_types;
  for (auto const *scope : existing_scopes) {
    for (auto const *type_stmt : SupStatementsOf(scope).Types) {
      for (auto const &[new_type, new_type_scope] : new_types) {
        RaiseIf<errors::SppIdentifierDuplicateError>(
          *new_type == *type_stmt->NewType, {new_type_scope, scope},
          ERR_ARGS(*new_type, *type_stmt->NewType, "associated type"));
      }
      new_types.EmplaceBack(type_stmt->NewType, scope);
    }
  }

  // Check for conflicting "cmp" statements.
  Vec<Pair<Shared<IdentifierAst>, Scope const*>> new_cmps;
  for (const auto *scope : existing_scopes) {
    for (auto const *cmp_stmt : SupStatementsOf(scope).Cmps) {
      for (auto const &[new_cmp, new_cmp_scope] : new_cmps) {
        RaiseIf<errors::SppIdentifierDuplicateError>(
          *new_cmp == *cmp_stmt->Name, {new_cmp_scope, scope},
          ERR_ARGS(*new_cmp, *cmp_stmt->Name, "comptime constant"));
      }
      new_cmps.EmplaceBack(cmp_stmt->Name, scope);
    }
  }
}

auto ScopeManager::CurrentIterator()-> ScopeIterator& {
  // Simple getter around the iterator.
  return _It;
}

auto ScopeManager::MakeSelfTypeSymbol(
  Scope *const linked_scope,
  Scope *const defined_in,
  const std::size_t pos)
  -> Shared<TypeSymbol> {
  // Create a "Self" symbol with a provided set of scopes.
  return MakeShared<TypeSymbol>(
    MakeUnique<TypeIdentifierAst>(pos, "Self", nullptr),
    nullptr, linked_scope, defined_in, TypeKind::Self);
}

auto ScopeManager::AddSelfTypeSymbol(
  Scope *const linked_scope,
  const std::size_t pos) const
  -> void {
  // Shortcut to create a Self symbol and inject it into
  // this scope. This scope is the "scope defined in", but
  // not necessarily the "linked scope" (provided).
  if (linked_scope == nullptr) { return; }
  CurrentScope->AddTypeSymbol(
    MakeSelfTypeSymbol(linked_scope, CurrentScope, pos));
}

auto ScopeManager::SyncSelfTypeSymbol(
  TypeAst const &cls_name) const-> void {
  using generate::common_types_precompiled::SELF_TYPE;
  // No work for $MockType values.
  if (cls_name.IsCompilerGeneratedType()) { return; }

  // Get the class symbol built off of the class name, in this
  // scope. Then, get the symbol for "Self", and copy the type
  // and LLVM info over from the "cls" symbol, into the "Self"
  // symbol.
  const auto cls_sym = CurrentScope->GetTypeSymbol(&cls_name);
  const auto self_sym = CurrentScope->GetTypeSymbol(SELF_TYPE.get(), true);
  self_sym->Type = cls_sym->Type;
  self_sym->LlvmInfo = cls_sym->LlvmInfo;
}

auto ScopeManager::Cleanup() -> void {
  // Clean up all static caches.
  Scope::OnInstantiationMissing = nullptr;
  normal_sup_blocks.clear();
  SupStatementsCache().clear();
  utils::type_members::ClearUnimplementedAbstractMethodsCache();
  generic_sup_blocks.Clear();
  temp_scopes.Clear();
  GenericParameterAst::ClearDummyScopes();
  ClosureExpressionAst::ClearMockAsts();
}

SPP_MOD_END
