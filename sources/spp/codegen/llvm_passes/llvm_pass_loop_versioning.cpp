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
      if (not IsAbortBlock(*succ)) { continue; }
      const auto reads_loop = genex::any_of(succ->front().operands(), [&](llvm::Use const &op) {
        return map.count(op.get()) > 0;
      });
      if (not reads_loop) { continue; }
      auto &copy = abort_copies[succ];
      if (copy == nullptr) {
        auto local = llvm::ValueToValueMapTy();
        copy = llvm::CloneBasicBlock(succ, local, suffix, succ->getParent());
        for (auto &inst : *copy) {
          llvm::RemapInstruction(&inst, map, llvm::RF_IgnoreMissingLocals | llvm::RF_NoModuleLevelChanges);
        }
      }
      copy_bb->getTerminator()->replaceSuccessorWith(succ, copy);
    }
  }
  dt.recalculate(*guard->getParent());
}
