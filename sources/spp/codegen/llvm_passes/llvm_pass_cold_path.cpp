#include <llvm/IR/CFG.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/ProfDataUtils.h>

#include <spp/macros.hpp>
#include <spp/codegen/llvm_passes.hpp>
import spp.utils.types;
import genex;

auto spp::codegen::ApplyColdPathHints(
  void *llvm_module) -> void {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  const auto marker = llvm_mod.getFunction(kColdPathMarker);
  if (marker == nullptr) { return; }

  // The weights "llvm.expect" gives a hinted branch, so
  // "unlikely(c)" and a "cold_path()" in its body weigh the
  // same.
  constexpr auto hot_weight = 2000u;
  constexpr auto cold_weight = 1u;

  // The functions holding a marker, each with the blocks its
  // markers sit in: the seed of that function's cold set.
  auto marked = Map<llvm::Function*, Set<llvm::BasicBlock*>>();
  auto calls = Vec<llvm::CallInst*>();
  for (const auto user : marker->users()) {
    const auto call = llvm::cast<llvm::CallInst>(user);
    marked[call->getFunction()].insert(call->getParent());
    calls.emplace_back(call);
  }

  // A successor the code is not expected to take: an unwind
  // edge, or a block ending in "unreachable" (such as the
  // dead fallthrough after an "else" arm, lowered as a branch
  // on "true"). Neither keeps a block hot.
  const auto never_taken = [](llvm::BasicBlock const *succ) {
    return succ->isEHPad() or llvm::isa<llvm::UnreachableInst>(succ->getTerminator());
  };

  // The successors a block continues through.
  const auto taken_succs = [&](llvm::BasicBlock const &bb) {
    auto succs = Vec<llvm::BasicBlock*>();
    const auto term = bb.getTerminator();
    for (auto i = 0u; i < term->getNumSuccessors(); ++i) {
      if (const auto succ = term->getSuccessor(i); not never_taken(succ)) {
        succs.emplace_back(succ);
      }
    }
    return succs;
  };

  for (auto &[fn, cold] : marked) {
    // Grow the cold set to a fixed point: a block with
    // somewhere to go, all of it cold, is cold. This reaches a
    // marker past a merge ("case a { case b {..} cold_path() }"),
    // which walking up from the marker would not.
    for (auto changed = true; changed;) {
      changed = false;
      for (auto &bb : *fn) {
        if (cold.contains(&bb)) { continue; }
        const auto succs = taken_succs(bb);
        if (succs.empty()) { continue; }
        if (genex::all_of(succs, [&](const auto succ) { return cold.contains(succ); })) {
          cold.insert(&bb);
          changed = true;
        }
      }
    }

    // Weight each branch or switch out of a hot block that
    // leads to a cold one, per successor. A branch out of a
    // cold block is left alone: the edge into that block
    // already carries the hint. A never-taken successor gets
    // the cold weight too, since weights given here replace
    // what llvm would otherwise have assumed about it.
    for (auto &bb : *fn) {
      if (cold.contains(&bb)) { continue; }
      const auto term = bb.getTerminator();
      if (not llvm::isa<llvm::CondBrInst>(term) and not llvm::isa<llvm::SwitchInst>(term)) { continue; }
      auto weights = llvm::SmallVector<std::uint32_t>();
      auto any_cold = false;
      for (auto i = 0u; i < term->getNumSuccessors(); ++i) {
        const auto succ = term->getSuccessor(i);
        const auto is_cold = cold.contains(succ);
        weights.emplace_back(is_cold or never_taken(succ) ? cold_weight : hot_weight);
        any_cold |= is_cold;
      }
      if (any_cold) { llvm::setBranchWeights(*term, weights, true); }
    }

    // Every path through the function is cold, so there is no
    // branch in it to weigh: the function itself is.
    if (cold.contains(&fn->getEntryBlock())) {
      fn->addFnAttr(llvm::Attribute::Cold);
    }
  }

  for (const auto call : calls) { call->eraseFromParent(); }
  marker->eraseFromParent();
}
