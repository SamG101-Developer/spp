#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/ValueMapper.h>

#include <spp/macros.hpp>
#include <spp/codegen/llvm_passes.hpp>
import spp.utils.types;
import genex;

auto spp::codegen::IsAbortBlock(
  llvm::BasicBlock const &bb) -> bool {
  const auto call = llvm::dyn_cast_or_null<llvm::CallInst>(bb.getTerminator()->getPrevNode());
  return llvm::isa<llvm::UnreachableInst>(bb.getTerminator()) and call != nullptr and call->doesNotReturn()
    and &bb.front() == call;
}

auto spp::codegen::IsAbortPath(
  llvm::BasicBlock const &bb, const llvm::function_ref<void(llvm::BasicBlock const &)> each) -> bool {
  // A float check's classifier, its infinity's test and three aborts are the
  // most there are.
  constexpr auto max_blocks = 8uz;
  auto seen = Set<llvm::BasicBlock const*>();
  auto work = Vec<llvm::BasicBlock const*>{&bb};
  while (not work.empty()) {
    const auto at = work.back();
    work.pop_back();
    if (not seen.insert(at).second) { continue; }
    if (seen.size() > max_blocks) { return false; }
    each(*at);
    if (IsAbortBlock(*at)) { continue; }
    if (llvm::succ_empty(at)) { return false; }
    if (genex::any_of(*at, [](llvm::Instruction const &i) { return i.mayHaveSideEffects(); })) { return false; }
    for (const auto succ : llvm::successors(at)) { work.emplace_back(succ); }
  }
  return true;
}

auto spp::codegen::VersionLoop(
  llvm::Loop &loop, llvm::BasicBlock *exit,
  llvm::function_ref<llvm::User*(llvm::Instruction *before)> make_condition,
  llvm::StringRef suffix, llvm::LoopInfo &li, llvm::DominatorTree &dt) -> void {
  // The preheader becomes the guard, and a new one the fast loop's.
  const auto guard = loop.getLoopPreheader();
  const auto preheader = llvm::SplitBlock(guard, guard->getTerminator(), &dt, &li, nullptr, "version.preheader");
  const auto condition = make_condition(guard->getTerminator());
  auto b = llvm::IRBuilder<>(guard->getTerminator());

  auto map = llvm::ValueToValueMapTy();
  auto blocks = llvm::SmallVector<llvm::BasicBlock*>();
  llvm::cloneLoopWithPreheader(preheader, guard, &loop, map, suffix, &li, &dt, blocks);
  llvm::remapInstructionsInBlocks(blocks, map);
  b.CreateCondBr(condition, preheader, llvm::cast<llvm::BasicBlock>(map[preheader]));
  guard->getTerminator()->eraseFromParent();

  // The copy leaves by the same blocks; the real exit's phis take its
  // values too. An abort block reading the loop's own values gets a
  // copy of its own, reading the copy's.
  auto abort_copies = llvm::DenseMap<llvm::BasicBlock*, llvm::BasicBlock*>();
  for (const auto bb : loop.blocks()) {
    const auto copy_bb = llvm::cast<llvm::BasicBlock>(map[bb]);
    for (const auto succ : llvm::successors(bb)) {
      if (loop.contains(succ)) { continue; }
      if (succ == exit) {
        for (auto &phi : exit->phis()) {
          const auto v = phi.getIncomingValueForBlock(bb);
          const auto mapped = static_cast<llvm::Value*>(map.lookup(v));
          phi.addIncoming(mapped != nullptr ? mapped : v, copy_bb);
        }
        continue;
      }

      // An abort path (an abort, or a float check's classifier before its
      // aborts) reading the loop's values gets a copy of its own, reading the
      // copy's; its phis (a classifier is an exit block, so in closed form it
      // reads the result through one) take the copy's blocks.
      auto path = Vec<llvm::BasicBlock*>();
      if (not IsAbortPath(*succ, [&](llvm::BasicBlock const &b) { path.emplace_back(const_cast<llvm::BasicBlock*>(&b)); })) {
        continue;
      }
      const auto reads_loop = genex::any_of(path, [&](llvm::BasicBlock const *b) {
        return genex::any_of(*b, [&](llvm::Instruction const &inst) {
          return llvm::isa<llvm::PHINode>(inst) or genex::any_of(inst.operands(), [&](llvm::Use const &op) {
            return map.count(op.get()) > 0;
          });
        });
      });
      if (not reads_loop) { continue; }
      auto &copy = abort_copies[succ];
      if (copy == nullptr) {
        auto local = llvm::ValueToValueMapTy();
        auto copies = Vec<llvm::BasicBlock*>();
        for (const auto b : path) {
          const auto c = llvm::CloneBasicBlock(b, local, suffix, succ->getParent());
          local[b] = c;
          copies.emplace_back(c);
        }
        for (const auto c : copies) {
          for (auto &inst : *c) {
            llvm::RemapInstruction(&inst, local, llvm::RF_IgnoreMissingLocals | llvm::RF_NoModuleLevelChanges);
            llvm::RemapInstruction(&inst, map, llvm::RF_IgnoreMissingLocals | llvm::RF_NoModuleLevelChanges);
          }
        }
        copy = copies.front();
      }
      // A phi in the original path now has only the original loop's edges.
      for (auto &phi : succ->phis()) {
        if (phi.getBasicBlockIndex(copy_bb) >= 0) { phi.removeIncomingValue(copy_bb, false); }
      }
      copy_bb->getTerminator()->replaceSuccessorWith(succ, copy);
    }
  }
  dt.recalculate(*guard->getParent());
}
