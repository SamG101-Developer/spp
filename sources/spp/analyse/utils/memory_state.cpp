module;
#include <spp/macros.hpp>
#include <spp/analyse/macros.hpp>

module spp.analyse.utils.memory_state;
import spp.analyse.errors.semantic_error;
import spp.analyse.errors.semantic_error_builder;
import spp.analyse.scopes.scope;
import spp.analyse.scopes.scope_manager;
import spp.analyse.scopes.symbols;
import spp.analyse.utils.control_flow;
import spp.analyse.utils.regions;
import spp.asts.ast;
import spp.asts.case_expression_branch_ast;
import spp.asts.case_pattern_variant_else_ast;
import spp.asts.identifier_ast;
import spp.asts.inner_scope_expression_ast;
import spp.asts.statement_ast;
import spp.asts.meta.compiler_meta_data;
import spp.codegen.llvm_alloca;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_layout;
import spp.codegen.llvm_type;
import spp.utils.ptr;
import genex;
import std;

namespace spp::analyse::utils::memory_state {
  namespace {
    /// Compare two escaping-borrow container lists by the memory
    /// regions they each name. Each branch of a "case" builds
    /// its own ast nodes, so the same borrow written in two
    /// branches is two pointers but one region, and only the
    /// region is that makes the branches disagree or agree.
    /// Todo: Verify this.
    auto EscapingBorrowContainersDiffer(
      Vec<Tup<Ast const*, Ast const*>> const &lhs,
      Vec<Tup<Ast const*, Ast const*>> const &rhs)
      -> bool {
      // The region converter takes the escaping borrows lists
      // and stringifies them, then sorts and compares.
      const auto regions = [](auto const &list) {
        auto out = Vec<Str>();
        for (auto const &[container, borrow] : list) {
          out.EmplaceBack(container->ToString() + " <- " + borrow->ToString());
        }
        genex::actions::sort(out);
        return out;
      };
      return regions(lhs) != regions(rhs);
    }

    /// The same for the handle's end: each branch borrows
    /// through its own ASTs (and scope), so pointers never
    /// match even when the branches borrow the same memory.
    template <typename T>
    auto ContainedEscapingBorrowsDiffer(T const &lhs, T const &rhs) -> bool {
      const auto regions = [](auto const &list) {
        auto out = Vec<Str>();
        for (auto const &[borrow, is_mut, _] : list) {
          out.EmplaceBack(borrow->ToString() + (is_mut ? " mut" : " ref"));
        }
        genex::actions::sort(out);
        return out;
      };
      return regions(lhs) != regions(rhs);
    }
  }
}

SPP_MOD_BEGIN
auto MemoryInfo::InitializedBy(
  Ast const &ast,
  Scope *scope)
  -> void {
  AstInitialization = {&ast, scope};
  AstInitializationOrigin = {&ast, scope};
  AstMoved = {nullptr, nullptr};
  InitializationCounter += 1;

  IsInconsistentlyInitialized = std::nullopt;
  IsInconsistentlyMoved = std::nullopt;
  IsInconsistentlyPartiallyMoved = std::nullopt;
  IsInconsistentlyBorrowEscaping = std::nullopt;
}

auto MemoryInfo::MovedBy(
  Ast const &ast, Scope *scope)
  -> void {
  AstMoved = {&ast, scope};
  AstInitialization = {nullptr, nullptr};
  AstPartialMoves.Clear();
}

auto MemoryInfo::RemovePartialMoves(
  Ast const &ast,
  Scope * /*scope*/)
  -> void {
  // Use "string" comparison; same as overlap checking mechanism.
  // Writing a moved-out part back puts only that part back: the
  // symbol is not re-initialised, which would clear what the
  // branches disagreed about while other parts are still missing.
  // Once no part is missing, none can be inconsistently missing.
  genex::actions::remove(
    AstPartialMoves, ast.ToString(),
    [](auto const &x) { return x->ToString(); });
  if (AstPartialMoves.IsEmpty()) {
    IsInconsistentlyPartiallyMoved = std::nullopt;
  }
}

auto MemoryInfo::Snapshot() const
  -> MemoryInfoSnapshot {
  // A snapshot is the saved part of this struct, so taking
  // one is just a copy of that part.
  return *this;
}

auto MemoryInfo::Clone() const
  -> Unique<MemoryInfo> {
  auto out = MakeUnique<MemoryInfo>();
  static_cast<MemoryState&>(*out) = *this;
  out->AstInitializationOrigin = AstInitializationOrigin;
  out->AstBorrowed = AstBorrowed;
  return out;
}

auto MemoryInfo::FillFromSnapshot(
  MemoryInfoSnapshot const &snapshot)
  -> void {
  // Everything a snapshot holds is the saved part of this
  // struct, and nothing outside it is touched.
  static_cast<MemoryState&>(*this) = snapshot;
}

SPP_MOD_END

auto spp::analyse::utils::memory_state::ValidateInconsistentMemory(
  Ast *parent, Vec<CaseExpressionBranchAst*> const &branches,
  VariableSymbol *const subject, ScopeManager *sm, CompilerMetaData *meta) -> void {
  // Define a simple alias for a list of symbols and their
  // memory.
  using SymbolMemoryList = Vec<Pair<CaseExpressionBranchAst*, memory_state::MemoryInfoSnapshot>>;
  using SymbolMemoryMap = Map<VariableSymbol*, memory_state::MemoryInfoSnapshot>;

  // Create a map of the symbols' memory  information before
  // any branches are analysed.
  auto sym_mem_info = Map<VariableSymbol*, SymbolMemoryList>();

  // The lookup walks ancestors and super scopes, which can
  // reach one symbol by more than one route, and every list
  // below is built with one entry per branch per occurrence.
  // Deduplicate.
  auto vs = Vec<VariableSymbol*>();
  auto seen_syms = Set<VariableSymbol*>();
  for (auto *sym : sm->CurrentScope->AllVarSymbols()) {
    if (seen_syms.insert(sym).second) { vs.EmplaceBack(sym); }
  }

  // The states before any branch has run. Each branch is restored
  // to these before the next one is analysed, and they stand in
  // as a final pseudo-branch for the consistency comparison below
  // - the same snapshot serving both, since nothing between the
  // two uses moves them apart.
  auto pre_analysis_mem_info = vs
    | genex::views::transform([](auto const &x) { return MakePair(x, x->MemInfo->Snapshot()); })
    | genex::to<Vec>();

  for (auto &&branch : branches) {
    // Analyse the memory and then recheck the symbols' memory
    // status.
    branch->Stage8_CheckMemory(sm, meta);

    // A branch binding parts off the subject takes the whole
    // of it - the "case" marks the subject moved once every
    // branch has run - so a part this branch left unbound is
    // a part nothing holds. Check all movable fields have been
    // bound, so dropping can take place.
    const auto branch_binds = subject != nullptr and genex::any_of(
      branch->Patterns, [](auto const &pattern) { return pattern->BindsByMove(); });
    if (branch_binds) {
      if (const auto skipped = regions::FirstUnaccountedPart(
        *subject, Vec<IdentifierAst*>{subject->Name.get()}, *sm); not skipped.empty()) {
        auto const *const blamed = branch->Patterns.IsEmpty()
          ? static_cast<Ast const*>(branch)
          : static_cast<Ast const*>(branch->Patterns[0].get());

        Raise<errors::SppDestructureSkipsOwnedPartError>(
          {sm->CurrentScope}, ERR_ARGS(*blamed, *subject->Name, StrView(skipped)));
      }
    }

    auto new_symbol_mem_info = vs
      | genex::views::transform([](auto const &x) { return MakePair(x, x->MemInfo->Snapshot()); })
      | genex::to<Vec>();

    // Reset the memory status of the symbols for the next branch
    // to analyse with the same original memory states.
    // Todo: Scopes need restoring properly too. (And rename to AstInit + Reformat).
    // Built once per branch rather than once per symbol: it is the same map every time round, and rebuilding it
    // inside the loop made recording one branch's states quadratic in the number of symbols in scope.
    auto new_symbol_mem_info_map = SymbolMemoryMap(new_symbol_mem_info.begin(), new_symbol_mem_info.end());

    for (auto &&[sym, old_mem_status] : pre_analysis_mem_info) {
      sym->MemInfo->FillFromSnapshot(old_mem_status);

      // Save this memory status for subsequent inter-branch
      // status comparisons.
      sym_mem_info[sym].EmplaceBack(branch, new_symbol_mem_info_map[sym]);
    }
  }

  // Add the pre-analysis memory states as a "final" branch
  // (just for comparison purposes).
  for (auto &&[sym, mem_info_list] : pre_analysis_mem_info) {
    sym_mem_info[sym].EmplaceBack(nullptr, std::move(mem_info_list));
  }

  // A branch that never finishes leaves no state behind to
  // agree with. Worked out once per branch, not per symbol:
  // it infers the body's type.
  auto diverging = Set<CaseExpressionBranchAst const*>();
  for (auto const *branch : branches) {
    if (control_flow::Diverges(*branch->Body, sm, meta)) { diverging.insert(branch); }
  }
  const auto diverges = [&](CaseExpressionBranchAst const *branch) { return diverging.contains(branch); };

  // Get the first "non-terminating" branch, and update the
  // symbols to reflect its memory state.
  const auto non_terminating_branch = genex::find_if(
    branches, [&](auto const &x) { return not diverges(x); });
  const auto first_branch = non_terminating_branch == branches.end() ? parent : *non_terminating_branch;
  const auto first_branch_index = non_terminating_branch != branches.end()
    ? genex::iterators::distance(branches.begin(), non_terminating_branch)
    : -1;
  const auto first_branch_mem_info_getter = [&](auto const &branch_mem_info) {
    return first_branch_index != -1
      ? branch_mem_info.At(static_cast<std::size_t>(first_branch_index)).second
      : branch_mem_info.Back().second;
  };

  const auto has_else_branch = not branches.IsEmpty()
    ? branches.Back()->Patterns[0]->To<CasePatternVariantElseAst>()
    : nullptr;
  const auto skip_else = has_else_branch and has_else_branch->MarkedForIterLoopExit();

  // Check for consistency among the branches' symbols' memory
  // states.
  for (auto const &[sym, branches_memory_info_lists] : sym_mem_info) {
    auto first_branch_mem_info = first_branch_mem_info_getter(branches_memory_info_lists);

    // Assuming all new memory states are consistent across
    // branches, update to the first "new" state list.
    sym->MemInfo->FillFromSnapshot(first_branch_mem_info);

    // Check the new memory status for each symbol is
    // consistent across all branches that don't terminate.
    // Without an "else", no branch may run at all, so the state
    // the "case" was entered with (the null entry) is one of the
    // paths leaving it too.
    auto applicable_branch_memory_info_lists = branches_memory_info_lists
      | genex::views::remove_if([&](auto const &x) {
        return (x.first == nullptr and has_else_branch) or (x.first != nullptr and diverges(x.first))
          or (skip_else and not branches.IsEmpty() and x.first == branches.Back());
      })
      | genex::to<Vec>();

    for (auto const &[branch_or_null, branch_memory_info_list] : applicable_branch_memory_info_lists) {
      Ast *const branch = branch_or_null != nullptr ? static_cast<Ast*>(branch_or_null) : parent;
      MarkInconsistentPaths(*sym, first_branch_mem_info, branch_memory_info_list, first_branch, branch);
    }
  }
}

auto spp::analyse::utils::memory_state::SnapshotSymbols(
  Vec<VariableSymbol*> const &syms)
  -> ScopeSnapshot {
  auto out = ScopeSnapshot();
  for (auto *sym : syms) { out.EmplaceBack(sym->SharedFromThis<VariableSymbol>(), sym->MemInfo->Snapshot()); }
  return out;
}

auto spp::analyse::utils::memory_state::SnapshotScopes(
  Scope const *from,
  Scope const *boundary)
  -> ScopeSnapshot {
  auto out = ScopeSnapshot();
  for (auto const *scope = from; scope != nullptr; scope = scope->Parent) {
    out.AppendRange(SnapshotSymbols(scope->AllVarSymbols(true)));
    if (scope == boundary) { break; }
  }
  return out;
}

auto spp::analyse::utils::memory_state::RestoreSnapshot(
  ScopeSnapshot const &snapshot)
  -> void {
  for (auto const &[sym, state] : snapshot) { sym->MemInfo->FillFromSnapshot(state); }
}

auto spp::analyse::utils::memory_state::MarkInconsistentPaths(
  VariableSymbol &sym,
  MemoryInfoSnapshot const &first,
  MemoryInfoSnapshot const &other,
  Ast *const first_path,
  Ast *const other_path)
  -> void {
  // A path that already disagreed within itself (a "case" nested
  // in it) still disagrees once it meets the others.
  auto &info = *sym.MemInfo;
  if (not info.IsInconsistentlyInitialized) { info.IsInconsistentlyInitialized = other.IsInconsistentlyInitialized; }
  if (not info.IsInconsistentlyMoved) { info.IsInconsistentlyMoved = other.IsInconsistentlyMoved; }
  if (not info.IsInconsistentlyPartiallyMoved) {
    info.IsInconsistentlyPartiallyMoved = other.IsInconsistentlyPartiallyMoved;
  }
  if (not info.IsInconsistentlyBorrowEscaping) {
    info.IsInconsistentlyBorrowEscaping = other.IsInconsistentlyBorrowEscaping;
  }

  if ((spp::get<0>(first.AstInitialization) == nullptr) != (spp::get<0>(other.AstInitialization) == nullptr)) {
    sym.MemInfo->IsInconsistentlyInitialized = {first_path, other_path};
  }
  if ((spp::get<0>(first.AstMoved) == nullptr) != (spp::get<0>(other.AstMoved) == nullptr)) {
    sym.MemInfo->IsInconsistentlyMoved = {first_path, other_path};
  }
  if (first.AstPartialMoves != other.AstPartialMoves) {
    sym.MemInfo->IsInconsistentlyPartiallyMoved = {first_path, other_path};
  }

  // Escaping borrows are compared from both ends of the link: a
  // symbol can be the coroutine handle that holds the borrows, or
  // the owner of the memory they borrow, and only the second is
  // what a later use of that memory (eg moving it) is checked
  // against.
  if (ContainedEscapingBorrowsDiffer(first.AstContainedEscapingBorrows, other.AstContainedEscapingBorrows)
    or EscapingBorrowContainersDiffer(first.AstContainersOfEscapingBorrows, other.AstContainersOfEscapingBorrows)) {
    sym.MemInfo->IsInconsistentlyBorrowEscaping = {first_path, other_path};
  }
}
