module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.sup_attachment;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_iterator;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.scope_tree;
import spp.analyse.scopes.substitution;
import spp.analyse.scopes.symbols;
import spp.analyse.scopes.type_key;
import spp.analyse.utils.monomorphization;
import spp.asts.ast;
import spp.asts.cmp_statement_ast;
import spp.asts.generic_parameter_ast;
import spp.asts.generic_parameter_group_ast;
import spp.asts.generic_parameter_type_constraints_ast;
import spp.asts.sup_prototype_extension_ast;
import spp.asts.sup_prototype_functions_ast;
import spp.asts.type_identifier_ast;
import spp.asts.type_statement_ast;
import spp.asts.generate.common_types_precompiled;
import spp.asts.meta.compiler_meta_data;
import spp.asts.utils.ast_utils;
import genex;
import std;

using namespace spp::analyse::scopes;
using namespace spp::asts;
using spp::asts::meta::CompilerMetaData;

namespace spp::analyse::utils::sup_attachment {
  namespace {

    /// Universal generic parameter grabber from "sup" and "sup-ext"
    /// blocks. The returning type is the same so easy to unify.
    auto GetSupGnParamsFromScope(
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
    /// generics and constraints, by identity: "pattern" is keyed
    /// where its parameters are parameters (the template block),
    /// and its constraints are read in "pattern_scope" (the block
    /// instantiated with what the first match bound). A generic
    /// type ("T" itself) is what the pattern binds instead.
    auto SupPatternApplies(
      TypeSymbol const &type_sym, const TypeId type, Scope const &type_scope,
      const TypeId pattern, Scope const &pattern_scope,
      GenericSubst &bindings,
      const bool check_constraints) -> bool {
      return type_sym.IsGn()
        ? UnifyTypeIds(pattern, type, pattern_scope, type_scope, bindings, false, check_constraints)
        : UnifyTypeIds(type, pattern, type_scope, pattern_scope, bindings, false, check_constraints);
    }

    /// Helper to detect if a sup block declares a variadic generic,
    /// or a generic "pack". Check the group for a variadic "..T"
    /// generic parameter.
    auto SupDeclaresAPack(
      Scope const &sup_scope)
      -> bool {
      // Get all generics and check for a variadic.
      const auto params = GetSupGnParamsFromScope(sup_scope);
      return params != nullptr and params->GetVariadicParam() != nullptr;
    }

    /// Check if any of the generics on a sup block contain
    /// constraints, which open up an additional check. If this is
    /// false, then an extra (expensive) check is avoided, which
    /// is why we bother checking in the first place.
    auto SupConstrainsItsParams(
      Scope const &sup_scope) -> bool {
      // Collect the generics on the sup block.
      auto const *params = GetSupGnParamsFromScope(sup_scope);
      if (params == nullptr) { return false; }

      // Check if any of the generics have constraints.
      return genex::any_of(params->GetTypeParams(), [](auto const *p) {
        return not p->TypeConstraints->Constraints.empty();
      });
    }

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
          found.Types.emplace_back(type_stmt);
        }
        else if (const auto cmp_stmt = member->To<CmpStatementAst>();
          cmp_stmt != nullptr and not cmp_stmt->Type->IsCompilerGeneratedType()) {
          found.Cmps.emplace_back(cmp_stmt);
        }
      }

      // Cache and return the mapping.
      return cache.emplace(scope, std::move(found)).first->second;
    }

    auto CheckConflictingTypeOrCmpStatements(
      TypeSymbol const &cls_sym,
      Scope const &sup_scope)
      -> void {
      // A "$" mock owns nothing that can conflict. It provides
      // overload-coalescing mock types.
      if (cls_sym.IsMock()) { return; }

      // Every block attached before this one was checked against
      // the others as it was attached, so only a pair involving
      // this block is new: one declaring neither a "type" nor a
      // "cmp" cannot conflict.
      if (auto const &mine = SupStatementsOf(&sup_scope); mine.Types.empty() and mine.Cmps.empty()) { return; }

      // The name will be the same for every block being considered:
      // blocks overlap when either's pattern takes the other's, by
      // identity, each read where it is written.
      const auto mine_name = AstName(sup_scope.AstNode);
      const auto mine_id = sup_scope.TypeIdOf(*mine_name);
      const auto existing_scopes = cls_sym.LinkedScope->DirectSupScopes
        | genex::views::filter([&](auto *scope) {
          return AstAs<SupPrototypeExtensionAst>(scope->AstNode)
            or AstAs<SupPrototypeFunctionsAst>(scope->AstNode);
        })
        | genex::views::filter([&](auto *scope) {
          auto const &theirs_scope = *scope->AstNode->GetAstScope();
          const auto theirs_id = theirs_scope.TypeIdOf(*AstName(scope->AstNode));
          if (mine_id == nullptr or theirs_id == nullptr) { return false; }
          auto fwd = GenericSubst();
          auto rev = GenericSubst();
          return UnifyTypeIds(mine_id, theirs_id, sup_scope, theirs_scope, fwd)
            or UnifyTypeIds(theirs_id, mine_id, theirs_scope, sup_scope, rev);
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
          new_types.emplace_back(type_stmt->NewType, scope);
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
          new_cmps.emplace_back(cmp_stmt->Name, scope);
        }
      }
    }

    auto CoalesceMethodMock(
          Scope &scope, ScopeManager const &sm) -> void {
      // Sanity guard on symbol type; we only want to hit $MockTypes
      // here, and do an ast check too.
      if (scope.LinkedTypeSymbol == nullptr or scope.LinkedTypeSymbol->Kind != TypeKind::FnMock
        or scope.Parent == nullptr) { return; }
      const auto sup_node = scope.Parent->AstNode;
      if (AstAs<SupPrototypeFunctionsAst>(sup_node) == nullptr and AstAs<SupPrototypeExtensionAst>(sup_node) == nullptr) {
        return;
      }

      // The owner's other sup-blocks. A block of an instantiation ("Box[S32]") has its own mock, so its siblings are the
      // instantiation's blocks, whose mocks are typed in its terms; a template's are the blocks registered for the type
      // being superimposed over. There might be none, in which case, return early too.
      auto owner_blocks = Vec<Scope*>();
      auto const *const self_sym = scope.Parent->FindSelfSymbol(true);
      auto *const owner_scope = self_sym != nullptr ? self_sym->LinkedScope : nullptr;
      if (owner_scope != nullptr and owner_scope->LinkedTypeSymbol != nullptr
        and owner_scope->LinkedTypeSymbol->InstanceOf != nullptr) {
        ignore = owner_scope->GetSupScopes();
        for (auto *const block : owner_scope->DirectSupScopes) {
          if (AstAs<SupPrototypeFunctionsAst>(block->AstNode) != nullptr
            or AstAs<SupPrototypeExtensionAst>(block->AstNode) != nullptr) { owner_blocks.emplace_back(block); }
        }
      }
      else if (const auto owner_sym = scope.Parent->FindHeadSymbol(*AstName(sup_node));
        owner_sym != nullptr) {
        if (const auto it = sm.Tree().NormalSupBlocks.find(owner_sym); it != sm.Tree().NormalSupBlocks.end()) {
          owner_blocks = it->second;
        }
      }
      if (owner_blocks.empty()) { return; }

      // Get the mock name, like $MockType for "fun mock_type()"
      // overloads, and begin iterating the matching sup blocks.
      const auto mock_name = scope.LinkedTypeSymbol->Name->WithoutGns();
      for (const auto block : owner_blocks) {
        if (block == scope.Parent) { continue; }
        const auto sibling = block->FindTypeSymbol(mock_name.get(), true, false);
        if (sibling == nullptr or sibling == scope.LinkedTypeSymbol.get()) { continue; }
        const auto sibling_blocks = sm.Tree().NormalSupBlocks.find(sibling);
        if (sibling_blocks == sm.Tree().NormalSupBlocks.end()) { continue; }

        for (auto *ext_scope : sibling_blocks->second) {
          const auto ext = AstAs<SupPrototypeExtensionAst>(ext_scope->AstNode);
          if (ext == nullptr or genex::contains(scope.DirectSupScopes, ext_scope)) { continue; }
          BumpTypeStructureGeneration();
          scope.DirectSupScopes.emplace_back(ext_scope);
          if (const auto fn_sym = ext_scope->FindTypeSymbol(ext->SuperCls.get());
            fn_sym != nullptr and fn_sym->LinkedScope != nullptr
            and not genex::contains(scope.DirectSupScopes, fn_sym->LinkedScope)) {
            scope.DirectSupScopes.emplace_back(fn_sym->LinkedScope);
          }
        }
      }
    }

    auto AttachSpecificSuperScopesImpl(
      Scope &scope, Vec<Scope*> const &sup_scopes, ScopeManager const &sm, CompilerMetaData *meta) -> void {
      using utils::monomorphization::MakeSupInstance;
      if (sup_scopes.empty()) { return; }

      // Clear the sup scopes list.
      BumpTypeStructureGeneration();
      scope.DirectSupScopes.clear();

      // Matched as a pattern: a template as itself over its own
      // parameters. For example, from Vec, get Vec[T=T]. Grab
      // other metadata.
      const auto fq_type = scope.LinkedTypeSymbol->GnSelfName();
      auto const &cls_sym = scope.LinkedTypeSymbol;
      const auto is_mock = scope.LinkedTypeSymbol->IsMock();
      auto const &type_scope = *scope.LinkedTypeSymbol->ScopeDefinedIn;
      auto type_id = type_scope.TypeIdOf(*fq_type);
      if (type_id == nullptr) { type_id = type_scope.TypeIdOfSymbol(*scope.LinkedTypeSymbol); }

      // Iterate through all the super scopes and check if the
      // name matches.
      for (const auto sup_scope : sup_scopes) {
        // Perform a relaxed comparison between the two types
        // (allows for specializations to match bases).
        auto bindings = GenericSubst();
        const auto own_mock_block = is_mock and not genex::contains(sm.Tree().GnSupBlocks, sup_scope);
        const auto pattern_id = sup_scope->TypeIdOf(*AstName(sup_scope->AstNode));

        // Bind the block's generics whilst checking that the
        // types match. Constraint checking is disabled here. A
        // side that does not key matches nothing.
        if (not own_mock_block and (type_id == nullptr or pattern_id == nullptr or not SupPatternApplies(
          *scope.LinkedTypeSymbol, type_id, type_scope, pattern_id, *sup_scope, bindings, false))) { continue; }

        // Only the block's own generics are its bindings: its pattern can name others' parameters too ("sup [T]
        // Vec[T]" matched against "Vec[Box[U]]", "U" a parameter of the type's own scope). A generic type is what the
        // pattern binds instead, so keeps all. A block over a pack ("sup [..Items] Tup[Items]") is attached with its pack
        // unbound, whatever the match bound it to: the block stands for every element set at once.
        const auto sup_params = GetSupGnParamsFromScope(*sup_scope);
        if (not scope.LinkedTypeSymbol->IsGn()) {
          bindings = sup_params != nullptr ? BindingsFor(bindings, *sup_params) : GenericSubst();
        }
        if (const auto params = sup_params; params != nullptr) {
          if (const auto pack = params->GetVariadicParam(); pack != nullptr) {
            const auto pid = pack->ParamId();
            std::erase_if(bindings.TypeParams, [pid](auto const &x) { return x.first == pid; });
            std::erase_if(bindings.CompParams, [pid](auto const &x) { return x.first == pid; });
            std::erase(bindings.TypePackParams, pid);
            std::erase(bindings.CompPackParams, pid);
          }
        }
        const auto scope_args_id = ArgsIdOfBindings(bindings, type_scope);

        // Create a generic version of the super scope if needed.
        auto new_sup_scope = static_cast<Scope*>(nullptr);
        auto new_cls_scope = static_cast<Scope*>(nullptr);
        auto sup_sym = static_cast<TypeSymbol*>(nullptr);

        if ((scope_args_id != nullptr
            or (SupDeclaresAPack(*sup_scope) and not AstName(sup_scope->AstNode)->IsCompilerGeneratedType()))
          and not genex::contains(sm.Tree().GnSupBlocks, sup_scope)) {
          // Build the generic sup scope for this instantiation,
          // and get the new cls symbol.
          tie(new_sup_scope, new_cls_scope) = MakeSupInstance(
            *sup_scope, scope, scope_args_id, &sm, meta);
          sup_sym = new_cls_scope ? new_cls_scope->LinkedTypeSymbol.get() : nullptr;

          // The constraint is checked here, against the constrained
          // type's super scopes - attached first if they are not yet.
          if (auto _ = GenericSubst(); not SupPatternApplies(
            *scope.LinkedTypeSymbol, type_id, type_scope, pattern_id, *new_sup_scope, _, true)) { continue; }
        }
        else {
          const auto sup_proto = AstAs<SupPrototypeExtensionAst>(
            sup_scope->AstNode);
          new_sup_scope = sup_scope;
          const auto sup_cls_sym = sup_proto
            ? sup_scope->FindTypeSymbol(sup_proto->SuperCls.get())
            : nullptr;
          new_cls_scope = sup_cls_sym ? sup_cls_sym->LinkedScope : nullptr;
          sup_sym = new_cls_scope ? new_cls_scope->LinkedTypeSymbol.get() : nullptr;

          // Nothing bound, so there is no substitution to record -
          // but a constraint declared here still has to be checked.
          // A variadic parameter is what reaches this: it stands
          // for a list of types, so the match binds it to nothing,
          // while "sup [..T: Copy] Tup[T]" still constrains every
          // element it swallowed.
          if (auto _ = GenericSubst(); SupConstrainsItsParams(*sup_scope) and type_id != nullptr and pattern_id != nullptr
            and not SupPatternApplies(*scope.LinkedTypeSymbol, type_id, type_scope, pattern_id, *new_sup_scope, _, true)) {
            continue;
          }
        }

        // Prevent double inheritance, cyclic inheritance and self
        // extension.
        if (const auto ext_ast = AstAs<SupPrototypeExtensionAst>(sup_scope->AstNode);
          ext_ast != nullptr and not ext_ast->Name->IsCompilerGeneratedType()) {
          // A super class that did not resolve under this instance's
          // arguments ("Try[.., Value=T]" with "T" bound to "Self",
          // parameter 0, still open) has nothing to be cyclic with.
          if (sup_sym != nullptr and sup_sym->LinkedScope != nullptr) {
            ext_ast->CheckCyclicExtension(*sup_sym, *sup_scope);
          }
          ext_ast->CheckDoubleExtension(*cls_sym, *sup_scope);
          ext_ast->CheckSelfExtension(*sup_scope);
        }

        // Register the super scope against the current scope.
        BumpTypeStructureGeneration();
        scope.DirectSupScopes.emplace_back(new_sup_scope);

        // Register the super scope's class scope against the current
        // scope, if it is different. This "difference" check ensures
        // that "sup [T] T ext A" doesn't create a "sup A ext A" link.
        const auto cls_scope_attached = new_cls_scope and scope.LinkedTypeSymbol != new_cls_scope->LinkedTypeSymbol;
        if (cls_scope_attached) {
          BumpTypeStructureGeneration();
          scope.DirectSupScopes.emplace_back(new_cls_scope);
        }

        // Check for conflicting "cmp" or "type" statements in the super
        // scopes.
        if (AstAs<SupPrototypeExtensionAst>(sup_scope->AstNode) or
          AstAs<SupPrototypeFunctionsAst>(sup_scope->AstNode)) {
          CheckConflictingTypeOrCmpStatements(*cls_sym, *sup_scope);
        }
      }
    }
  }
}

auto spp::analyse::utils::sup_attachment::AttachSpecificSuperScopes(
  Scope &scope, ScopeManager const &sm, CompilerMetaData *meta) -> bool {
  // Handle type symbols, each once. Marked first, so a scope
  // reached again while its own attachment runs (a cycle) is
  // not attached twice.
  if (scope.LinkedTypeSymbol == nullptr or scope.SupsAttached) { return false; }
  if (scope.LinkedTypeSymbol->Kind == TypeKind::GnTypeParam) { return false; }
  scope.SupsAttached = true;
  const auto non_generic_sym = TypeRef::ForKindCheck(scope).Template();

  // Get the sup blocks for the type if there are any.
  const auto it = sm.Tree().NormalSupBlocks.find(non_generic_sym);
  const auto normal = it != sm.Tree().NormalSupBlocks.end()
    ? &it->second
    : nullptr;

  // A pure generic block ("sup [T] T") names no type, so it
  // applies to every one of them and has to be merged in.
  // There are usually none at all, and then the stored list
  // is handed over as it stands.
  if (sm.Tree().GnSupBlocks.empty()) {
    if (normal != nullptr) { AttachSpecificSuperScopesImpl(scope, *normal, sm, meta); }
  }
  else {
    auto scopes = normal != nullptr ? *normal : Vec<Scope*>();
    scopes.append_range(sm.Tree().GnSupBlocks);
    AttachSpecificSuperScopesImpl(scope, scopes, sm, meta);
  }

  // Merge the overloads that appear in different sup-blocks
  // into one (shared) $MockType.
  CoalesceMethodMock(scope, sm);
  return true;
}

auto spp::analyse::utils::sup_attachment::AttachAllSuperScopes(
  ScopeManager &sm, CompilerMetaData *meta) -> void {
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
  // A read can come from the middle of any analysis (keying a type, a lookup inside a qualified name), and the attach
  // is not part of it: it starts from a clean context, not the reader's ("TypeAnalysisTypeScope" pointing elsewhere).
  Scope::OnSupScopesRead = [&sm, meta](Scope const &read) {
    if (read.SupsAttached or read.LinkedTypeSymbol == nullptr) { return; }
    const auto _meta_guard = asts::meta::MetaGuard(meta, true);
    meta->ResetContext();
    ignore = AttachSpecificSuperScopes(const_cast<Scope&>(read), sm, meta);
  };

  // The sweep creates scopes as it runs - attaching a
  // constrained sup instantiates the substituted type - so
  // it repeats until a pass attaches nothing new.
  for (auto found_new = true; found_new;) {
    found_new = false;
    sm.RestartWalk();
    for (auto it = ScopeIterator(sm.CurrentScope); it != ScopeIterator(); ++it) {
      if (AttachSpecificSuperScopes(**it, sm, meta)) { found_new = true; }
    }
  }
  sm.RestartWalk();
}

auto spp::analyse::utils::sup_attachment::ClearSupStatementsCache() -> void {
  SupStatementsCache().clear();
}
