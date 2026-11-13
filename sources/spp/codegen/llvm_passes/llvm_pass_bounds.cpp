#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/ScalarEvolutionExpressions.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Transforms/Utils/LCSSA.h>
#include <llvm/Transforms/Utils/LoopSimplify.h>
#include <llvm/Transforms/Utils/ScalarEvolutionExpander.h>

#include <spp/macros.hpp>
#include <spp/codegen/llvm_passes.hpp>
import spp.utils.types;
import genex;

namespace spp::codegen {
  namespace {
    /// One index check of a loop, as what keeps it from firing on the
    /// rounds the loop runs: its branch, and "Last" (the induction
    /// variable's value on the last of them, worked out at twice its
    /// width so it cannot wrap) against "Len" (at the same width):
    /// "Last u< Len" ("Strict") or "Last u<= Len". The variable only
    /// rises, so the last value passing is every value passing.
    struct IndexCheck {
      llvm::CondBrInst *Branch;
      unsigned Stay;
      llvm::SCEV const *Last;
      llvm::SCEV const *Len;
      bool Strict;
    };
  }

  /// The runtime's number for a failed index check ("RuntimeCheck::Index"
  /// in "llvm_fn_impls.cpp", the runtime's SPPC_CHECK_INDEX).
  static constexpr auto kIndexCheckKind = 1u;

  /// The largest loop versioned, in instructions: a copy of it is
  /// made, so a big loop body is left checked rather than doubled.
  static constexpr auto kMaxLoopInstructions = 400uz;

  /// Whether "abort" is where a failed index check goes: a call to
  /// "sppc_check_failed" with the index kind.
  static auto IsIndexAbort(llvm::BasicBlock const &abort) -> bool;

  /// The check "br" makes, if it is an index check the loop's trip
  /// count ("count", the rounds after the first) can prove: an exit to
  /// an index abort taken when an induction variable rising by a
  /// constant reaches a length the loop does not change ("i == len",
  /// "i u>= len", "i u> len", either way round).
  static auto IndexCheckOf(
    llvm::CondBrInst *br, llvm::Loop const &loop, llvm::SCEV const *count, llvm::ScalarEvolution &se)
    -> Opt<IndexCheck>;

  namespace {
    /// Version loops by their index checks; see "AddBoundsPasses". The
    /// guard tests, for every check, that the last round's value passes
    /// ("Last u< Len", at twice the width): a proof by comparison, never
    /// by a no-wrap flag, so no check is elided on an assumption (safety
    /// plan S4.4). A guard known true needs no copy: the checks are just
    /// dropped.
    struct VersionIndexChecksPass : llvm::PassInfoMixin<VersionIndexChecksPass> {
      static auto run(llvm::Function &fn, llvm::FunctionAnalysisManager &fam) -> llvm::PreservedAnalyses {
        auto &li = fam.getResult<llvm::LoopAnalysis>(fn);
        auto &se = fam.getResult<llvm::ScalarEvolutionAnalysis>(fn);
        auto &dt = fam.getResult<llvm::DominatorTreeAnalysis>(fn);

        // Collected first, as versioning adds loops.
        auto loops = Vec<llvm::Loop*>();
        for (const auto loop : li.getLoopsInPreorder()) {
          if (loop->isInnermost() and loop->getLoopPreheader() != nullptr and loop->getLoopLatch() != nullptr) {
            loops.emplace_back(loop);
          }
        }

        auto changed = false;
        for (const auto loop : loops) {
          // One real exit, whose trip count is known, and index checks.
          auto exiting = static_cast<llvm::BasicBlock*>(nullptr);
          auto exit = static_cast<llvm::BasicBlock*>(nullptr);
          auto real_exits = 0uz;
          auto size = 0uz;
          for (const auto bb : loop->blocks()) {
            size += bb->size();
            for (const auto succ : llvm::successors(bb)) {
              if (loop->contains(succ) or IsAbortBlock(*succ)) { continue; }
              ++real_exits;
              exiting = bb;
              exit = succ;
            }
          }
          if (real_exits != 1 or size > kMaxLoopInstructions) { continue; }
          const auto count = se.getExitCount(loop, exiting);
          if (llvm::isa<llvm::SCEVCouldNotCompute>(count)) { continue; }

          auto checks = Vec<IndexCheck>();
          for (const auto bb : loop->blocks()) {
            const auto br = llvm::dyn_cast<llvm::CondBrInst>(bb->getTerminator());
            if (br == nullptr) { continue; }
            if (const auto check = IndexCheckOf(br, *loop, count, se); check.has_value()) { checks.emplace_back(*check); }
          }
          if (checks.empty()) { continue; }

          // The guard: every check passes on every round. Already known?
          const auto passes = [&](IndexCheck const &c) {
            return se.isKnownPredicate(c.Strict ? llvm::ICmpInst::ICMP_ULT : llvm::ICmpInst::ICMP_ULE, c.Last, c.Len);
          };
          if (not genex::all_of(checks, passes)) {
            auto expander = llvm::SCEVExpander(se, "bounds");
            const auto expandable = genex::all_of(checks, [&](IndexCheck const &c) {
              return expander.isSafeToExpand(c.Last) and expander.isSafeToExpand(c.Len);
            });
            if (not expandable) { continue; }
            VersionLoop(*loop, exit, [&](llvm::Instruction *before) -> llvm::User* {
              auto b = llvm::IRBuilder<>(before);
              auto all = static_cast<llvm::Value*>(b.getTrue());
              for (auto const &c : checks) {
                const auto at = before->getIterator();
                const auto ty = c.Last->getType();
                const auto last = expander.expandCodeFor(c.Last, ty, at);
                const auto len = expander.expandCodeFor(c.Len, ty, at);
                const auto fits = c.Strict
                  ? b.CreateICmpULT(last, len, "bounds.fits") : b.CreateICmpULE(last, len, "bounds.fits");
                all = b.CreateAnd(all, fits, "bounds.all");
              }
              return llvm::cast<llvm::User>(all);
            }, ".bounds.checked", li, dt);
          }

          // The fast copy ("loop" itself) stays in it at every check.
          for (auto const &c : checks) {
            const auto stay = c.Branch->getSuccessor(c.Stay);
            const auto abort = c.Branch->getSuccessor(1u - c.Stay);
            abort->removePredecessor(c.Branch->getParent());
            llvm::UncondBrInst::Create(stay, c.Branch->getIterator());
            c.Branch->eraseFromParent();
          }
          se.forgetLoop(loop);
          changed = true;
        }
        return changed ? llvm::PreservedAnalyses::none() : llvm::PreservedAnalyses::all();
      }
    };
  }
}

auto spp::codegen::IsIndexAbort(llvm::BasicBlock const &abort) -> bool {
  if (not IsAbortBlock(abort)) { return false; }
  auto const &call = llvm::cast<llvm::CallInst>(abort.front());
  const auto callee = call.getCalledFunction();
  if (callee == nullptr or callee->getName() != "sppc_check_failed" or call.arg_size() == 0) { return false; }
  const auto kind = llvm::dyn_cast<llvm::ConstantInt>(call.getArgOperand(0));
  return kind != nullptr and kind->getZExtValue() == kIndexCheckKind;
}

auto spp::codegen::IndexCheckOf(
  llvm::CondBrInst *br, llvm::Loop const &loop, llvm::SCEV const *count, llvm::ScalarEvolution &se)
  -> Opt<IndexCheck> {
  const auto cmp = llvm::dyn_cast<llvm::ICmpInst>(br->getCondition());
  if (cmp == nullptr) { return Nullopt; }
  const auto out = loop.contains(br->getSuccessor(0)) ? 1u : 0u;
  if (loop.contains(br->getSuccessor(out)) or not IsIndexAbort(*br->getSuccessor(out))) { return Nullopt; }

  // The predicate under which the abort is taken, the induction
  // variable on the left.
  auto pred = out == 0 ? cmp->getPredicate() : cmp->getInversePredicate();
  auto iv = se.getSCEV(cmp->getOperand(0));
  auto len = se.getSCEV(cmp->getOperand(1));
  if (not llvm::isa<llvm::SCEVAddRecExpr>(iv)) {
    std::swap(iv, len);
    pred = llvm::ICmpInst::getSwappedPredicate(pred);
  }
  const auto ar = llvm::dyn_cast<llvm::SCEVAddRecExpr>(iv);
  if (ar == nullptr or ar->getLoop() != &loop or not ar->isAffine() or not se.isLoopInvariant(len, &loop)) {
    return Nullopt;
  }
  const auto step = llvm::dyn_cast<llvm::SCEVConstant>(ar->getStepRecurrence(se));
  if (step == nullptr or not step->getAPInt().isStrictlyPositive()) { return Nullopt; }
  if (pred == llvm::ICmpInst::ICMP_EQ and not step->getAPInt().isOne()) { return Nullopt; }
  if (pred != llvm::ICmpInst::ICMP_EQ and pred != llvm::ICmpInst::ICMP_UGE and pred != llvm::ICmpInst::ICMP_UGT) {
    return Nullopt;
  }

  // Its value on the last round, "start + step * count", at twice
  // its width: at most (2^w - 1) + (2^w - 1)^2, which fits, so the
  // wide value is the true one and "Last u< Len" also says it never
  // wrapped.
  const auto width = static_cast<unsigned>(se.getTypeSizeInBits(ar->getType()));
  if (se.getTypeSizeInBits(count->getType()) > width) { return Nullopt; }
  const auto wide = llvm::IntegerType::get(br->getContext(), width * 2);
  const auto last = se.getAddExpr(
    se.getZeroExtendExpr(ar->getStart(), wide),
    se.getMulExpr(se.getZeroExtendExpr(step, wide), se.getZeroExtendExpr(count, wide)));
  return IndexCheck{
    .Branch = br, .Stay = 1u - out, .Last = last, .Len = se.getZeroExtendExpr(len, wide),
    .Strict = pred != llvm::ICmpInst::ICMP_UGT};
}

auto spp::codegen::AddBoundsPasses(
  llvm::FunctionPassManager &fpm) -> void {
  // Versioning needs each loop's preheader and single latch, and closed
  // form (a value leaving the loop only through the exit's phis), so the
  // copy's values can join the original's at the exit.
  fpm.addPass(llvm::LoopSimplifyPass());
  fpm.addPass(llvm::LCSSAPass());
  fpm.addPass(VersionIndexChecksPass());
  fpm.addPass(llvm::LoopSimplifyPass());
  fpm.addPass(llvm::LCSSAPass());
}
