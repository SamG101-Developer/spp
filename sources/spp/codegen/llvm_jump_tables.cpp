module;
#include <spp/macros.hpp>

module spp.codegen.llvm_jump_tables;
import spp.codegen.llvm_support;
import spp.codegen.llvm_target;
import spp.utils.types;
import genex;
import llvm;
import std;

namespace spp::codegen {
  namespace {
    /// When a switch gets a table: LLVM's own defaults for building
    /// one ("-min-jump-table-entries" and "-jump-table-density").
    constexpr auto kMinCases = 4uz;
    constexpr auto kMinDensityPercent = 10uz;

    /// The most entries a table may have, so a switch whose range is
    /// huge but still dense enough is left as compares rather than
    /// given a table of megabytes.
    constexpr auto kMaxEntries = std::uint64_t{1} << 16;

    /// The function attribute that builds a function's switches
    /// without LLVM's own (unhardened) jump tables.
    constexpr auto kNoJumpTablesAttr = llvm::StringLiteral("no-jump-tables");

    /// Todo: the table holds absolute block addresses, which need a
    ///  relocation each (so it lives in ".data.rel.ro", read-only once
    ///  RELRO applies). Entries relative to the table, as LLVM's own
    ///  are, would halve it and need none.

    /// The smallest case value of a switch, and how many entries its
    /// table has from there: the values "Min" to "Min + Entries - 1".
    struct TableRange {
      llvm::APInt Min;
      std::uint64_t Entries;
    };

    /// The range a dense enough switch's table covers, or nothing for
    /// one that stays compares. Case values are ordered as signed, as
    /// LLVM's own switch lowering orders them.
    auto TableRangeOf(llvm::SwitchInst const &sw) -> Opt<TableRange> {
      const auto cases = static_cast<std::size_t>(sw.getNumCases());
      const auto width = sw.getCondition()->getType()->getIntegerBitWidth();
      if (cases < kMinCases or width > 64) { return Nullopt; }

      auto min = sw.case_begin()->getCaseValue()->getValue();
      auto max = min;
      for (auto const &c : sw.cases()) {
        auto const &v = c.getCaseValue()->getValue();
        if (v.slt(min)) { min = v; }
        if (v.sgt(max)) { max = v; }
      }
      const auto span = (max - min).getZExtValue();
      if (span >= kMaxEntries) { return Nullopt; }
      const auto entries = span + 1;
      if (cases * 100 < entries * kMinDensityPercent) { return Nullopt; }
      return TableRange{.Min = std::move(min), .Entries = entries};
    }

    /// Where an out-of-range value goes: the switch's default, or, for a
    /// default that is "unreachable" (an exhaustive "case"), a trap, so
    /// a speculative path through the table's last entry lands nowhere
    /// harmful.
    auto DefaultTargetOf(llvm::SwitchInst &sw) -> llvm::BasicBlock* {
      const auto dflt = sw.getDefaultDest();
      if (not llvm::isa<llvm::UnreachableInst>(dflt->getFirstNonPHIOrDbg())) { return dflt; }
      auto &fn = *sw.getFunction();
      const auto trap = llvm::BasicBlock::Create(fn.getContext(), "jt.trap", &fn);
      auto b = llvm::IRBuilder<>(trap);
      b.CreateIntrinsic(llvm::Intrinsic::trap, {});
      b.CreateUnreachable();
      return trap;
    }

    /// Whether "sw"'s condition provably holds one of its case range's
    /// values, "min" to "max" (as LLVM's value-range analysis reads it):
    /// then its table index needs no mask, as no value, even a
    /// mispredicted one, can be out of range.
    auto IsAlwaysInRange(llvm::SwitchInst const &sw, TableRange const &range) -> bool {
      auto max = range.Min + (range.Entries - 1);
      const auto cases = llvm::ConstantRange::getNonEmpty(range.Min, max + 1);
      const auto query = llvm::SimplifyQuery(sw.getModule()->getDataLayout(), &sw);
      return cases.contains(llvm::computeConstantRange(sw.getCondition(), false, query));
    }

    /// Replace "sw" with its table and an "indirectbr" through it.
    ///
    /// The table's slot 0 is the default, and the value "min + i" is
    /// at slot "i + 1", so the index is "zext(cond - min) + 1", which
    /// never wraps, and an index out of range is masked to 0 with no
    /// branch: "cmp", "sbb", "and" on x86. The mask is skipped when the
    /// condition cannot be out of range ("IsAlwaysInRange", or cases
    /// covering every value of its type). The backend folds the "+ 1"
    /// into the table's address.
    auto LowerToTable(llvm::SwitchInst &sw, TableRange const &range, std::size_t const index) -> void {
      auto &fn = *sw.getFunction();
      auto &ctx = fn.getContext();
      const auto bb = sw.getParent();
      const auto i64 = llvm::Type::getInt64Ty(ctx);
      const auto ptr_ty = llvm::PointerType::get(ctx, 0);
      const auto dflt = DefaultTargetOf(sw);

      // The table: the default, then one entry per value in range.
      auto targets = Vec<llvm::BasicBlock*>(range.Entries + 1, dflt);
      for (auto const &c : sw.cases()) {
        targets[1 + (c.getCaseValue()->getValue() - range.Min).getZExtValue()] = c.getCaseSuccessor();
      }
      auto entries = Vec<llvm::Constant*>();
      for (const auto target : targets) { entries.emplace_back(llvm::BlockAddress::get(&fn, target)); }
      const auto table_ty = llvm::ArrayType::get(ptr_ty, entries.size());
      const auto table = new llvm::GlobalVariable(
        *fn.getParent(), table_ty, true, llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantArray::get(table_ty, entries), fn.getName() + ".jt." + std::to_string(index));
      table->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);

      // The index, masked to 0 (the default) when out of range. The
      // mask goes through an empty asm, so the backend cannot see it
      // as a select and turn it into a branch.
      auto b = llvm::IRBuilder<>(&sw);
      const auto cond = sw.getCondition();
      const auto rel = b.CreateSub(cond, llvm::ConstantInt::get(cond->getType(), range.Min), "jt.rel");
      auto idx = b.CreateAdd(b.CreateZExtOrTrunc(rel, i64), llvm::ConstantInt::get(i64, 1), "jt.idx");
      if (not IsAlwaysInRange(sw, range)) {
        const auto bound = llvm::ConstantInt::get(i64, range.Entries + 1);
        const auto in_range = b.CreateSExt(b.CreateICmpULT(idx, bound, "jt.in_range"), i64, "jt.mask.raw");
        const auto launder = llvm::InlineAsm::get(llvm::FunctionType::get(i64, {i64}, false), "", "=r,0", false);
        const auto mask = b.CreateCall(launder, {in_range}, "jt.mask");
        idx = b.CreateAnd(idx, mask, "jt.index");
      }
      const auto slot = b.CreateInBoundsGEP(table_ty, table, {llvm::ConstantInt::get(i64, 0), idx}, "jt.slot");
      const auto target = b.CreateLoad(ptr_ty, slot, "jt.target");

      // Each target once. A phi had one entry per edge from "bb" (a
      // successor several cases share has several); it keeps one if
      // its block is still a target, and none if not (an unreachable
      // default the trap replaced).
      auto unique = Vec<llvm::BasicBlock*>();
      for (const auto t : targets) {
        if (genex::find(unique, t) == unique.end()) { unique.emplace_back(t); }
      }
      auto edges = Map<llvm::BasicBlock*, std::size_t>();
      for (auto i = 0u; i < sw.getNumSuccessors(); ++i) { ++edges[sw.getSuccessor(i)]; }
      for (auto &[succ, count] : edges) {
        const auto keep = genex::find(unique, succ) != unique.end() ? 1uz : 0uz;
        for (auto k = keep; k < count; ++k) { succ->removePredecessor(bb, true); }
      }

      const auto br = b.CreateIndirectBr(target, static_cast<unsigned>(unique.size()));
      for (const auto t : unique) { br->addDestination(t); }
      sw.eraseFromParent();
    }
  }
}

auto spp::codegen::ApplyHardenedJumpTables(
  void *llvm_module) -> void {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  for (auto &fn : llvm_mod) {
    if (not fn.isDeclaration()) { fn.addFnAttr(kNoJumpTablesAttr, "true"); }
  }
  if (not DoesTargetSupportIndirectBranches()) { return; }

  // Collected first, as lowering one replaces it.
  for (auto &fn : llvm_mod) {
    if (fn.isDeclaration()) { continue; }
    auto switches = Vec<llvm::SwitchInst*>();
    for (auto &bb : fn) {
      if (const auto sw = llvm::dyn_cast<llvm::SwitchInst>(bb.getTerminator()); sw != nullptr) {
        switches.emplace_back(sw);
      }
    }
    auto index = 0uz;
    for (const auto sw : switches) {
      if (const auto range = TableRangeOf(*sw); range.has_value()) { LowerToTable(*sw, *range, index++); }
    }
  }
}
