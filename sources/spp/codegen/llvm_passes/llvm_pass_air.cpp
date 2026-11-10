#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/Analysis/MemoryLocation.h>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/IR/ConstantRange.h>
#include <llvm/IR/Dominators.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/IR/Operator.h>
#include <llvm/IR/PatternMatch.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/LCSSA.h>
#include <llvm/Transforms/Utils/LoopSimplify.h>
#include <llvm/Transforms/Utils/Mem2Reg.h>
#include <llvm/Transforms/Utils/ScalarEvolutionExpander.h>

#include <spp/macros.hpp>
#include <spp/codegen/llvm_passes.hpp>

import spp.utils.types;
import genex;

namespace spp::codegen {
  namespace {
    /// The narrowest vector registers the AIR passes act for. Below
    /// them (baseline x86-64's SSE2: two 64-bit lanes, and no 64-bit
    /// arithmetic shift or compare) a vectorised check costs about
    /// what the branching one it replaces did, and a loop's setup
    /// more: measured, a deferred in-place add saved 3.5%, an exact
    /// sum 4.8%, and a 16-element window got 10% slower.
    constexpr auto kAirMinVectorBits = 256u;

    /// Whether "fn"'s target has vectors wide enough to be worth the
    /// AIR passes. Asked per function, so one built for wider vectors
    /// than the program's default is judged by its own.
    auto IsWideEnough(llvm::Function &fn, llvm::FunctionAnalysisManager &fam) -> bool {
      auto const &tti = fam.getResult<llvm::TargetIRAnalysis>(fn);
      return tti.getRegisterBitWidth(llvm::TargetTransformInfo::RGK_FixedWidthVector).getFixedValue() >= kAirMinVectorBits;
    }

    /// The checked intrinsic "check" is the overflow test of "r"
    /// ("x op y") for, if any: a "*.with.overflow" of the same op and
    /// operands, or a comparison true for exactly the "x" (and "y")
    /// the op overflows for. InstCombine makes the comparisons, from an
    /// intrinsic whose value went unused once "EmitCheckedArith"
    /// returned the flagged op instead: LLVM's add-overflow shapes for
    /// two variables ("m_UAddWithOverflow"), "x u< y" for an unsigned
    /// subtract, and, for a constant operand, any test of "x" whose
    /// truth set is the op's overflow set (a range: "x u> MAX - 1",
    /// "x == MAX", ...), compared as ranges so no shape is missed. A
    /// subtract of a constant may have become "x + -C".
    struct FusedCheck {
      llvm::Intrinsic::ID Id;
      llvm::Value *Rhs;
    };

    auto OverflowCheckOf(llvm::BinaryOperator const *r, llvm::Instruction const *check) -> Opt<FusedCheck> {
      namespace pm = llvm::PatternMatch;
      const auto x = r->getOperand(0);
      const auto y = r->getOperand(1);
      const auto opcode = r->getOpcode();

      // An intrinsic of the same op and operands, read only through
      // "extractvalue" (so its parts can be handed over).
      if (const auto w = llvm::dyn_cast<llvm::WithOverflowInst>(check)) {
        if (not genex::all_of(w->users(), [](auto u) { return llvm::isa<llvm::ExtractValueInst>(u); })) { return Nullopt; }
        const auto same = (w->getLHS() == x and w->getRHS() == y)
          or (r->isCommutative() and w->getLHS() == y and w->getRHS() == x);
        if (w->getBinaryOp() == opcode and same) { return FusedCheck{.Id = w->getIntrinsicID(), .Rhs = y}; }
        return Nullopt;
      }
      const auto cmp = llvm::dyn_cast<llvm::ICmpInst>(check);
      if (cmp == nullptr) { return Nullopt; }

      // Two variables: an unsigned add's shapes, or "x u< y".
      if (opcode == llvm::Instruction::Add and (
        pm::match(cmp, pm::m_UAddWithOverflow(pm::m_Specific(x), pm::m_Specific(y), pm::m_Value()))
        or pm::match(cmp, pm::m_UAddWithOverflow(pm::m_Specific(y), pm::m_Specific(x), pm::m_Value())))) {
        return FusedCheck{.Id = llvm::Intrinsic::uadd_with_overflow, .Rhs = y};
      }
      if (opcode == llvm::Instruction::Sub and (
        pm::match(cmp, pm::m_SpecificICmp(llvm::ICmpInst::ICMP_ULT, pm::m_Specific(x), pm::m_Specific(y)))
        or pm::match(cmp, pm::m_SpecificICmp(llvm::ICmpInst::ICMP_UGT, pm::m_Specific(y), pm::m_Specific(x))))) {
        return FusedCheck{.Id = llvm::Intrinsic::usub_with_overflow, .Rhs = y};
      }

      // A constant operand: the test's truth set against the op's
      // overflow set, for each check the op could have come from.
      const auto c = llvm::dyn_cast<llvm::ConstantInt>(y);
      if (c == nullptr) { return Nullopt; }
      // The test is on "x" itself, or on "x + C" (InstCombine's form of
      // a range test, as a multiply's is): then "x"'s truth set is the
      // comparison's shifted back by "C".
      const auto offset_of = [&](llvm::Value *v) -> Opt<llvm::APInt> {
        if (v == x) { return llvm::APInt::getZero(x->getType()->getIntegerBitWidth()); }
        llvm::APInt const *off = nullptr;
        if (pm::match(v, pm::m_Add(pm::m_Specific(x), pm::m_APInt(off)))) { return *off; }
        return Nullopt;
      };
      auto k = static_cast<llvm::ConstantInt*>(nullptr);
      auto pred = cmp->getPredicate();
      auto offset = Opt<llvm::APInt>();
      if (offset = offset_of(cmp->getOperand(0)); offset.has_value()) {
        k = llvm::dyn_cast<llvm::ConstantInt>(cmp->getOperand(1));
      }
      else if (offset = offset_of(cmp->getOperand(1)); offset.has_value()) {
        k = llvm::dyn_cast<llvm::ConstantInt>(cmp->getOperand(0));
        pred = llvm::ICmpInst::getSwappedPredicate(pred);
      }
      if (k == nullptr) { return Nullopt; }
      const auto truth = llvm::ConstantRange::makeExactICmpRegion(pred, k->getValue()).subtract(*offset);
      const auto overflows = [&](const llvm::Instruction::BinaryOps op, llvm::APInt const &v, const unsigned kind) {
        return llvm::ConstantRange::makeExactNoWrapRegion(op, v, kind).inverse() == truth;
      };
      constexpr auto nuw = llvm::OverflowingBinaryOperator::NoUnsignedWrap;
      constexpr auto nsw = llvm::OverflowingBinaryOperator::NoSignedWrap;
      const auto &v = c->getValue();
      switch (opcode) {
        case llvm::Instruction::Add:
          if (overflows(llvm::Instruction::Add, v, nuw)) { return FusedCheck{.Id = llvm::Intrinsic::uadd_with_overflow, .Rhs = y}; }
          if (overflows(llvm::Instruction::Add, v, nsw)) { return FusedCheck{.Id = llvm::Intrinsic::sadd_with_overflow, .Rhs = y}; }
          if (overflows(llvm::Instruction::Sub, -v, nuw)) {
            return FusedCheck{.Id = llvm::Intrinsic::usub_with_overflow, .Rhs = llvm::ConstantInt::get(c->getType(), -v)};
          }
          return Nullopt;
        case llvm::Instruction::Sub:
          if (overflows(llvm::Instruction::Sub, v, nuw)) { return FusedCheck{.Id = llvm::Intrinsic::usub_with_overflow, .Rhs = y}; }
          if (overflows(llvm::Instruction::Sub, v, nsw)) { return FusedCheck{.Id = llvm::Intrinsic::ssub_with_overflow, .Rhs = y}; }
          return Nullopt;
        case llvm::Instruction::Mul:
          if (overflows(llvm::Instruction::Mul, v, nuw)) { return FusedCheck{.Id = llvm::Intrinsic::umul_with_overflow, .Rhs = y}; }
          if (overflows(llvm::Instruction::Mul, v, nsw)) { return FusedCheck{.Id = llvm::Intrinsic::smul_with_overflow, .Rhs = y}; }
          return Nullopt;
        default:
          return Nullopt;
      }
    }

    /// One overflow check: the branch to its abort, and either the
    /// checked add or subtract whose bit it branches on ("Op"), or a
    /// comparison ("Bit") InstCombine made of one, the overflow test of a
    /// flagged plain op ("OverflowCheckOf"): a multiply by a constant,
    /// whose check is the multiplicand outside a range, which unlike the
    /// intrinsic vectorises. "Results" are the flagged ops whose values
    /// are wrong when it fails.
    struct Check {
      llvm::CondBrInst *Branch;
      llvm::WithOverflowInst *Op;
      llvm::Value *Bit;
      Vec<llvm::BinaryOperator*> Results;
    };

    /// The checked add or subtract whose overflow bit "cond" is.
    auto CheckedAddSubOf(llvm::Value *cond) -> llvm::WithOverflowInst* {
      const auto ev = llvm::dyn_cast<llvm::ExtractValueInst>(cond);
      if (ev == nullptr or ev->getIndices()[0] != 1) { return nullptr; }
      const auto op = llvm::dyn_cast<llvm::WithOverflowInst>(ev->getAggregateOperand());
      return op != nullptr and op->getBinaryOp() != llvm::Instruction::Mul ? op : nullptr;
    }

    /// A loop's overflow checks, and the one edge out of it that is
    /// not one: "Exiting" to "Exit".
    struct LoopExits {
      Vec<Check> Checks;
      llvm::BasicBlock *Exiting;
      llvm::BasicBlock *Exit;
    };

    /// The flagged plain op(s) codegen pairs with "op" as its result
    /// ("EmitCheckedArith"): the same operation on the same operands,
    /// flagged "nsw" if "op" is signed or "nuw" if not, exact wherever
    /// the check passed. A user's own "*_wrapping" op has no flag, so
    /// it is never taken for one.
    auto FlaggedResultsOf(llvm::WithOverflowInst const *op) -> Vec<llvm::BinaryOperator*> {
      auto found = Vec<llvm::BinaryOperator*>();
      const auto x = op->getLHS();
      const auto y = op->getRHS();
      for (const auto user : x->users()) {
        const auto bo = llvm::dyn_cast<llvm::BinaryOperator>(user);
        if (bo == nullptr or bo->getOpcode() != op->getBinaryOp()) { continue; }
        const auto same = (bo->getOperand(0) == x and bo->getOperand(1) == y)
          or (bo->isCommutative() and bo->getOperand(0) == y and bo->getOperand(1) == x);
        const auto flagged = op->isSigned() ? bo->hasNoSignedWrap() : bo->hasNoUnsignedWrap();
        if (same and flagged) { found.emplace_back(bo); }
      }
      return found;
    }

    /// The runtime's numbers for the overflow checks ("RuntimeCheck" in
    /// "llvm_fn_impls.cpp"): signed and unsigned add, subtract and
    /// multiply. Division's are not among them: a division must not run
    /// with the divisor its check refused.
    constexpr auto kFirstOverflowKind = 0x10u;
    constexpr auto kLastOverflowKind = 0x15u;

    /// Whether "abort" is where a failed overflow check goes.
    auto IsOverflowAbort(llvm::BasicBlock const &abort) -> bool {
      if (not IsAbortBlock(abort)) { return false; }
      auto const &call = llvm::cast<llvm::CallInst>(abort.front());
      const auto callee = call.getCalledFunction();
      if (callee == nullptr or callee->getName() != "sppc_check_failed" or call.arg_size() == 0) { return false; }
      const auto kind = llvm::dyn_cast<llvm::ConstantInt>(call.getArgOperand(0));
      return kind != nullptr and kind->getZExtValue() >= kFirstOverflowKind and kind->getZExtValue() <= kLastOverflowKind;
    }

    /// The overflow check "br" (to an overflow abort) makes, if it is
    /// one this pass can defer: the bit of a checked add or subtract, or
    /// a comparison that is the overflow test of a flagged add, subtract
    /// or multiply in "loop".
    auto CheckOf(llvm::CondBrInst *br, llvm::Loop const &loop) -> Opt<Check> {
      if (const auto op = CheckedAddSubOf(br->getCondition()); op != nullptr) {
        return Check{.Branch = br, .Op = op, .Bit = nullptr, .Results = FlaggedResultsOf(op)};
      }
      const auto cmp = llvm::dyn_cast<llvm::ICmpInst>(br->getCondition());
      if (cmp == nullptr or not loop.contains(cmp) or not IsOverflowAbort(*br->getSuccessor(0))) { return Nullopt; }
      for (const auto bb : loop.blocks()) {
        for (auto &inst : *bb) {
          const auto r = llvm::dyn_cast<llvm::BinaryOperator>(&inst);
          if (r == nullptr or not r->getType()->isIntegerTy() or not r->hasPoisonGeneratingFlags()) { continue; }
          const auto op = r->getOpcode();
          if (op != llvm::Instruction::Add and op != llvm::Instruction::Sub and op != llvm::Instruction::Mul) { continue; }
          if (OverflowCheckOf(r, cmp).has_value()) {
            return Check{.Branch = br, .Op = nullptr, .Bit = cmp, .Results = {r}};
          }
        }
      }
      return Nullopt;
    }

    /// The overflow checks of "loop", if every edge out of it but one
    /// is such a check: a branch on the bit of a checked add or
    /// subtract to an abort block whose arguments come from outside
    /// the loop, so a copy of it can run at the exit instead. Any
    /// other way out (a bounds check, a second real exit) would keep
    /// the loop from vectorising anyway.
    auto FindLoopChecks(llvm::Loop const &loop) -> Opt<LoopExits> {
      auto exits = LoopExits{.Checks = {}, .Exiting = nullptr, .Exit = nullptr};
      auto real_exits = 0uz;
      for (const auto bb : loop.blocks()) {
        for (const auto succ : llvm::successors(bb)) {
          if (loop.contains(succ)) { continue; }
          const auto br = llvm::dyn_cast<llvm::CondBrInst>(bb->getTerminator());
          const auto check = br != nullptr and br->getSuccessor(0) == succ and IsAbortBlock(*succ)
            ? CheckOf(br, loop) : Nullopt;
          const auto movable = check.has_value() and genex::none_of(
            llvm::cast<llvm::CallInst>(succ->front()).args(), [&](llvm::Use const &arg) {
              const auto inst = llvm::dyn_cast<llvm::Instruction>(arg.get());
              return inst != nullptr and loop.contains(inst);
            });
          if (movable) { exits.Checks.emplace_back(*check); }
          else { ++real_exits; exits.Exiting = bb; exits.Exit = succ; }
        }
      }
      if (exits.Checks.empty() or real_exits != 1) { return Nullopt; }
      return exits;
    }

    /// Whether "loop" does nothing anyone else could see: no calls
    /// but checked arithmetic and markers, and no volatile or atomic
    /// memory access.
    auto HasNothingObservable(llvm::Loop const &loop) -> bool {
      for (const auto bb : loop.blocks()) {
        for (auto const &inst : *bb) {
          if (const auto call = llvm::dyn_cast<llvm::CallBase>(&inst)) {
            const auto id = call->getIntrinsicID();
            if (not llvm::isa<llvm::WithOverflowInst>(call) and id != llvm::Intrinsic::lifetime_start
              and id != llvm::Intrinsic::lifetime_end and id != llvm::Intrinsic::assume) { return false; }
          }
          else if (inst.isAtomic() or llvm::isa<llvm::FenceInst>(inst)) { return false; }
          else if (const auto ld = llvm::dyn_cast<llvm::LoadInst>(&inst); ld != nullptr and not ld->isSimple()) {
            return false;
          }
          else if (const auto st = llvm::dyn_cast<llvm::StoreInst>(&inst); st != nullptr and not st->isSimple()) {
            return false;
          }
        }
      }
      return true;
    }

    /// Whether "ld" may read what one of "stores" wrote.
    auto MayReadBack(llvm::LoadInst const &ld, Vec<llvm::StoreInst const*> const &stores, llvm::AAResults &aa) -> bool {
      const auto loc = llvm::MemoryLocation::get(&ld);
      return genex::any_of(stores, [&](auto st) { return not aa.isNoAlias(loc, llvm::MemoryLocation::get(st)); });
    }

    /// Whether a wrapped result of "checks" stays harmless until the
    /// loop's exit. Each result, everything computed from it, and
    /// every load that may read one back ("tainted") may feed only
    /// more arithmetic, casts, selects (not their condition), other
    /// deferred checks (an op or a comparison: a tainted input means a
    /// check has already failed, whose flag aborts at the exit whatever
    /// this one says) and stored values: never a phi (so nothing is
    /// carried round in a register, which would also keep the loop
    /// from vectorising), a branch, an address, a division or a call.
    /// Each op must be read only through "extractvalue", and by one
    /// check, so it can be rewritten once.
    auto IsTaintContained(llvm::Loop const &loop, Vec<Check> const &checks, llvm::AAResults &aa) -> bool {
      auto ops = Set<llvm::Value const*>();
      auto bits = Set<llvm::Value const*>();
      auto branches = Set<llvm::Value const*>();
      auto tainted = Set<llvm::Value const*>();
      auto work = Vec<llvm::Value const*>();
      auto stores = Vec<llvm::StoreInst const*>();
      const auto taint = [&](llvm::Value const *v) { if (tainted.insert(v).second) { work.emplace_back(v); } };

      for (auto const &check : checks) {
        branches.insert(check.Branch);
        if (check.Op != nullptr) {
          if (not ops.insert(check.Op).second) { return false; }
          for (const auto user : check.Op->users()) {
            const auto ev = llvm::dyn_cast<llvm::ExtractValueInst>(user);
            if (ev == nullptr) { return false; }
            if (ev->getIndices()[0] == 0) { taint(ev); }
          }
        }
        if (check.Bit != nullptr) { bits.insert(check.Bit); }
        for (const auto result : check.Results) { taint(result); }
      }

      while (not work.empty()) {
        while (not work.empty()) {
          const auto v = work.back();
          work.pop_back();
          for (const auto user : v->users()) {
            const auto inst = llvm::cast<llvm::Instruction>(user);
            if (not loop.contains(inst) or branches.contains(inst)) { continue; }
            if (const auto st = llvm::dyn_cast<llvm::StoreInst>(inst)) {
              if (st->getPointerOperand() == v) { return false; }
              stores.emplace_back(st);
              continue;
            }
            const auto passes = (llvm::isa<llvm::BinaryOperator>(inst) and not inst->isIntDivRem())
              or ops.contains(inst) or bits.contains(inst) or llvm::isa<llvm::CastInst, llvm::FreezeInst>(inst)
              or (llvm::isa<llvm::ExtractValueInst>(inst) and ops.contains(inst->getOperand(0)))
              or (llvm::isa<llvm::SelectInst>(inst) and llvm::cast<llvm::SelectInst>(inst)->getCondition() != v);
            if (not passes) { return false; }
            taint(inst);
          }
        }

        // A load that may read a tainted value back is tainted too;
        // round again until nothing new is.
        for (const auto bb : loop.blocks()) {
          for (auto const &inst : *bb) {
            const auto ld = llvm::dyn_cast<llvm::LoadInst>(&inst);
            if (ld != nullptr and not tainted.contains(ld) and MayReadBack(*ld, stores, aa)) { taint(ld); }
          }
        }
      }
      return true;
    }

    /// The checks of "loop" that can be deferred to its exit, or none.
    /// Deferring pays only if the loop then vectorises (in a scalar
    /// loop a branching check is nearly free, a deferred one is not),
    /// so only loops that can are taken: innermost, with a bound on
    /// their trip count (a loop that only ever ends by overflowing
    /// would otherwise run forever instead of aborting), whose only
    /// other exits are the checks, that do nothing observable, and
    /// whose wrapped results stay harmless until the exit.

    auto FindDeferredChecks(llvm::Loop const &loop, llvm::ScalarEvolution &se, llvm::AAResults &aa) -> Opt<Vec<Check>> {
      if (not loop.isInnermost() or loop.getLoopPreheader() == nullptr) { return Nullopt; }
      if (llvm::isa<llvm::SCEVCouldNotCompute>(se.getSymbolicMaxBackedgeTakenCount(&loop))) { return Nullopt; }
      auto exits = FindLoopChecks(loop);
      if (not exits or not HasNothingObservable(loop) or not IsTaintContained(loop, exits->Checks, aa)) { return Nullopt; }
      return exits->Checks;
    }

    /// "op", a checked add or subtract, as plain arithmetic, with its
    /// overflow bit as a comparison: the vectoriser cannot widen
    /// "*.with.overflow", which returns a struct. Its flagged results
    /// lose their flags: with the check moved to the loop's exit, one
    /// that wrapped is used before it, and must be the wrapped value,
    /// not poison. Answers the bit.
    auto LowerCheckedOp(llvm::WithOverflowInst *op) -> llvm::Value* {
      for (const auto result : FlaggedResultsOf(op)) { result->dropPoisonGeneratingFlags(); }
      auto b = llvm::IRBuilder<>(op);
      const auto x = op->getLHS();
      const auto y = op->getRHS();
      const auto add = op->getBinaryOp() == llvm::Instruction::Add;
      const auto value = add ? b.CreateAdd(x, y, "air.value") : b.CreateSub(x, y, "air.value");
      const auto overflowed = not op->isSigned()
        ? b.CreateICmpULT(add ? value : x, add ? x : y, "air.overflowed")
        : b.CreateICmpSLT(
          add ? b.CreateAnd(b.CreateXor(x, value), b.CreateXor(y, value))
          : b.CreateAnd(b.CreateXor(x, y), b.CreateXor(x, value)),
          llvm::Constant::getNullValue(x->getType()), "air.overflowed");
      for (const auto user : Vec<llvm::User*>(op->user_begin(), op->user_end())) {
        const auto ev = llvm::cast<llvm::ExtractValueInst>(user);
        ev->replaceAllUsesWith(ev->getIndices()[0] == 0 ? value : overflowed);
        ev->eraseFromParent();
      }
      op->eraseFromParent();
      return overflowed;
    }

    /// Route "loop"'s one exit through a test of each flag in turn: a
    /// set flag aborts through a copy of its fail block, so the first
    /// kind to have overflowed is the one reported.
    auto TestFlagsAtExit(
      llvm::Loop const &loop, Vec<Pair<llvm::BasicBlock*, llvm::AllocaInst*>> const &flags, llvm::Function &fn)
      -> void {
      auto exiting = static_cast<llvm::BasicBlock*>(nullptr);
      auto exit = static_cast<llvm::BasicBlock*>(nullptr);
      for (const auto bb : loop.blocks()) {
        for (const auto succ : llvm::successors(bb)) {
          if (not loop.contains(succ)) { exiting = bb; exit = succ; }
        }
      }

      auto tests = Vec<llvm::BasicBlock*>();
      for (auto i = 0uz; i < flags.size(); ++i) {
        tests.emplace_back(llvm::BasicBlock::Create(fn.getContext(), "air.exit", &fn, exit));
      }
      for (auto i = 0uz; i < flags.size(); ++i) {
        auto map = llvm::ValueToValueMapTy();
        const auto abort = llvm::CloneBasicBlock(flags[i].first, map, ".air", &fn);
        auto b = llvm::IRBuilder<>(tests[i]);
        b.CreateCondBr(
          b.CreateLoad(b.getInt1Ty(), flags[i].second, "air.set"), abort, i + 1 < tests.size() ? tests[i + 1] : exit);
      }
      exiting->getTerminator()->replaceSuccessorWith(exit, tests.front());
      exit->replacePhiUsesWith(exiting, tests.back());
    }

    /// Defer "checks" to "loop"'s exit: each becomes plain arithmetic
    /// whose bit is or-ed into its fail block's flag instead of
    /// branching, and the exit tests the flags. The flags are stack
    /// slots, cleared before the loop, for "mem2reg" to promote.
    auto DeferChecks(llvm::Loop const &loop, Vec<Check> const &checks, llvm::Function &fn) -> void {
      auto entry = llvm::IRBuilder<>(&fn.getEntryBlock(), fn.getEntryBlock().getFirstInsertionPt());
      auto pre = llvm::IRBuilder<>(loop.getLoopPreheader()->getTerminator());
      auto flags = Vec<Pair<llvm::BasicBlock*, llvm::AllocaInst*>>();
      const auto flag_of = [&](llvm::BasicBlock *fail) {
        const auto found = genex::find_if(flags, [&](auto const &f) { return f.first == fail; });
        if (found != flags.end()) { return found->second; }
        const auto flag = entry.CreateAlloca(entry.getInt1Ty(), nullptr, "air.flag");
        pre.CreateStore(pre.getFalse(), flag);
        flags.emplace_back(fail, flag);
        return flag;
      };

      for (auto const &[br, op, bit, results] : checks) {
        const auto flag = flag_of(br->getSuccessor(0));
        auto overflowed = bit;
        if (op != nullptr) { overflowed = LowerCheckedOp(op); }
        else { for (const auto result : results) { result->dropPoisonGeneratingFlags(); } }
        auto b = llvm::IRBuilder<>(br);
        b.CreateStore(b.CreateOr(b.CreateLoad(b.getInt1Ty(), flag), overflowed, "air.any"), flag);
        b.CreateBr(br->getSuccessor(1));
        br->eraseFromParent();
      }
      TestFlagsAtExit(loop, flags, fn);
    }

    /// One checked running sum: "Acc" in the loop's header, "Next"
    /// ("Acc" plus "Addend", flagged) coming round, checked by "Op"
    /// and its branch "Check" each time round.
    struct ExactSum {
      llvm::PHINode *Acc;
      llvm::BinaryOperator *Next;
      llvm::WithOverflowInst *Op;
      llvm::Value *Addend;
      llvm::CondBrInst *Check;
    };

    /// A loop's checked running sums, its one real exit edge, and a
    /// bound on how many times its back edge is taken.
    struct ExactLoop {
      llvm::Loop *Loop;
      Vec<ExactSum> Sums;
      llvm::BasicBlock *Exiting;
      llvm::BasicBlock *Exit;
      llvm::SCEV const *Count;
    };

    /// The most iterations the split sums stay exact for: each part
    /// of an addend is at most 32 bits, so 2^32 of them fit 64 bits.
    constexpr auto kExactSumMaxBackedges = 0xFFFFFFFFull;

    /// The fewest iterations worth an exact sum: below it, the setup
    /// and the one check of the total outweigh what vectorising saves
    /// (a 16-element window was 12% slower), and the loop keeps its
    /// cheap branching checks.
    constexpr auto kExactSumMinBackedges = 31ull;

    /// The checked running sums of "loop" that can be computed
    /// exactly instead, or none. The loop must be one the deferral
    /// rules would otherwise take but for its sums: innermost, its
    /// trip count bounded, its only other exits overflow checks, and
    /// nothing observable in it, so finishing the loop before the one
    /// check of the total cannot be seen. A sum is "Acc" in the header
    /// with "Acc + v" (a checked add, up to 64 bits) coming round from
    /// the latch, where "Acc" feeds only that add and the new value
    /// only "Acc" and the exit's phis.
    auto FindExactSums(llvm::Loop &loop, llvm::ScalarEvolution &se) -> Opt<ExactLoop> {
      if (not loop.isInnermost() or loop.getLoopPreheader() == nullptr or loop.getLoopLatch() == nullptr) {
        return Nullopt;
      }
      const auto count = se.getSymbolicMaxBackedgeTakenCount(&loop);
      if (llvm::isa<llvm::SCEVCouldNotCompute>(count)) { return Nullopt; }
      auto exits = FindLoopChecks(loop);
      if (not exits or not HasNothingObservable(loop)) { return Nullopt; }

      auto found = ExactLoop{.Loop = &loop, .Sums = {}, .Exiting = exits->Exiting, .Exit = exits->Exit, .Count = count};
      for (auto &acc : loop.getHeader()->phis()) {
        const auto ty = llvm::dyn_cast<llvm::IntegerType>(acc.getType());
        const auto next = llvm::dyn_cast<llvm::BinaryOperator>(acc.getIncomingValueForBlock(loop.getLoopLatch()));
        if (ty == nullptr or ty->getBitWidth() > 64 or next == nullptr or next->getOpcode() != llvm::Instruction::Add) {
          continue;
        }

        // The check of this add: a checked add of the same operands,
        // whose flagged result "Next" is. Only the intrinsic form: a
        // comparison check has no op to rebuild the sum from.
        const auto check = genex::find_if(exits->Checks, [&](Check const &c) {
          return c.Op != nullptr and c.Op->getBinaryOp() == llvm::Instruction::Add
            and genex::contains(FlaggedResultsOf(c.Op), next);
        });
        if (check == exits->Checks.end()) { continue; }
        const auto op = check->Op;
        if (op->getLHS() == op->getRHS() or (op->getLHS() != &acc and op->getRHS() != &acc)) { continue; }

        // "Acc" feeds only the sum and its check; the new value goes
        // round to "Acc" or out to the exit's phis (the loop is in
        // closed form); the op is read only for its bit, and the bit
        // only by the check.
        const auto acc_ok = genex::all_of(acc.users(), [&](llvm::User const *u) { return u == next or u == op; });
        const auto next_ok = genex::all_of(next->users(), [&](llvm::User const *u) {
          const auto phi = llvm::dyn_cast<llvm::PHINode>(u);
          return u == &acc or (phi != nullptr and phi->getParent() == exits->Exit);
        });
        const auto op_ok = genex::all_of(op->users(), [&](llvm::User const *u) {
          const auto ev = llvm::dyn_cast<llvm::ExtractValueInst>(u);
          return ev != nullptr and (ev->use_empty() or (ev->getIndices()[0] == 1 and ev->hasOneUser()
            and ev->user_back() == check->Branch));
        });
        if (not acc_ok or not next_ok or not op_ok) { continue; }
        found.Sums.emplace_back(&acc, next, op, op->getLHS() == &acc ? op->getRHS() : op->getLHS(), check->Branch);
      }
      if (found.Sums.empty()) { return Nullopt; }
      return found;
    }

    /// Give "loop" a copy that keeps its checks, taken unless it goes
    /// round between "kExactSumMinBackedges" and
    /// "kExactSumMaxBackedges" times: the trip count ("count") is
    /// worked out before it, and picks between the two. False, and
    /// nothing changed, if it cannot be.
    auto VersionByTripCount(
      llvm::Loop &loop, llvm::BasicBlock *exit, llvm::SCEV const *count, llvm::ScalarEvolution &se,
      llvm::LoopInfo &li, llvm::DominatorTree &dt) -> bool {
      auto expander = llvm::SCEVExpander(se, "air");
      if (not expander.isSafeToExpand(count)) { return false; }
      const auto ty = count->getType();
      VersionLoop(loop, exit, [&](llvm::Instruction *before) -> llvm::User* {
        auto b = llvm::IRBuilder<>(before);
        const auto counted = expander.expandCodeFor(count, ty, before->getIterator());
        return llvm::cast<llvm::User>(b.CreateAnd(
          b.CreateICmpUGE(counted, llvm::ConstantInt::get(ty, kExactSumMinBackedges)),
          ty->getIntegerBitWidth() <= 32
            ? b.getTrue() : b.CreateICmpULE(counted, llvm::ConstantInt::get(ty, kExactSumMaxBackedges)),
          "air.exact.worth"));
      }, ".air.checked", li, dt);
      return true;
    }

    /// Compute "found"'s sums exactly, with no check inside the loop.
    /// Each addend goes into plain sums that cannot overflow within
    /// "kExactSumMaxBackedges" rounds: at 64 bits for one of 32 bits
    /// or fewer, and split into its low 32 bits (unsigned) and high 32
    /// bits (signed or not, as the sum is) for one of 64. On the way
    /// out, the exact total (the start, plus the parts) is assembled
    /// at 128 bits and checked against the sum's type once, aborting
    /// as the old check did if it does not fit: so a sum whose total
    /// fits never stops, whatever its partial sums did.
    auto RewriteExactSums(ExactLoop const &found, llvm::Function &fn) -> void {
      auto &ctx = fn.getContext();
      const auto loop = found.Loop;
      const auto preheader = loop->getLoopPreheader();
      const auto latch = loop->getLoopLatch();
      const auto i64 = llvm::Type::getInt64Ty(ctx);
      const auto i128 = llvm::Type::getInt128Ty(ctx);

      // The exit edge gets a block of its own, where the totals go.
      auto cur = llvm::BasicBlock::Create(ctx, "air.sum.exit", &fn, found.Exit);
      found.Exiting->getTerminator()->replaceSuccessorWith(found.Exit, cur);

      for (auto const &[acc, next, op, addend, check] : found.Sums) {
        const auto width = acc->getType()->getIntegerBitWidth();
        const auto is_signed = op->isSigned();
        const auto fail = check->getSuccessor(0);
        const auto start = acc->getIncomingValueForBlock(preheader);

        // The parts, each a plain sum round the loop.
        auto b = llvm::IRBuilder<>(op);
        const auto ext = [&](llvm::Value *v, llvm::Type *ty) {
          return is_signed ? b.CreateSExt(v, ty) : b.CreateZExt(v, ty);
        };
        auto parts = Vec<Pair<llvm::Value*, unsigned>>();
        const auto part = [&](llvm::Value *add, const unsigned shift, const bool part_signed) {
          const auto phi = llvm::PHINode::Create(i64, 2, "air.sum.part", loop->getHeader()->begin());
          const auto sum = b.CreateAdd(phi, add, "air.sum.next", not part_signed, part_signed);
          phi->addIncoming(llvm::ConstantInt::get(i64, 0), preheader);
          phi->addIncoming(sum, latch);
          parts.emplace_back(sum, shift);
        };
        if (width <= 32) { part(ext(addend, i64), 0, is_signed); }
        else {
          part(b.CreateAnd(addend, 0xFFFFFFFFull, "air.sum.low"), 0, false);
          part(is_signed ? b.CreateAShr(addend, 32, "air.sum.high") : b.CreateLShr(addend, 32, "air.sum.high"), 32, is_signed);
        }

        // The total, checked once, on the way out.
        auto e = llvm::IRBuilder<>(cur);
        auto total = is_signed ? e.CreateSExt(start, i128) : e.CreateZExt(start, i128);
        for (auto const &[sum, shift] : parts) {
          const auto wide = (is_signed and (width <= 32 or shift != 0)) ? e.CreateSExt(sum, i128) : e.CreateZExt(sum, i128);
          total = e.CreateAdd(total, shift == 0 ? wide : e.CreateShl(wide, shift), "air.sum.total");
        }
        const auto result = e.CreateTrunc(total, acc->getType(), "air.sum.result");
        const auto fits = is_signed
          ? e.CreateICmpEQ(e.CreateSExt(result, i128), total, "air.sum.fits")
          : e.CreateICmpULT(total, llvm::ConstantInt::get(i128, llvm::APInt::getOneBitSet(128, width)), "air.sum.fits");
        auto map = llvm::ValueToValueMapTy();
        const auto next_bb = llvm::BasicBlock::Create(ctx, "air.sum.exit", &fn, found.Exit);
        e.CreateCondBr(fits, next_bb, llvm::CloneBasicBlock(fail, map, ".air", &fn));

        // The exit's phis take the exact result in place of the sum.
        for (auto &phi : found.Exit->phis()) {
          for (auto i = 0u; i < phi.getNumIncomingValues(); ++i) {
            if (phi.getIncomingBlock(i) == found.Exiting and phi.getIncomingValue(i) == next) {
              phi.setIncomingValue(i, result);
            }
          }
        }

        // The old sum, its check and its op go.
        llvm::IRBuilder<>(check).CreateBr(check->getSuccessor(1));
        check->eraseFromParent();
        for (const auto user : Vec<llvm::User*>(op->user_begin(), op->user_end())) {
          llvm::cast<llvm::Instruction>(user)->eraseFromParent();
        }
        op->eraseFromParent();
        acc->replaceAllUsesWith(llvm::PoisonValue::get(acc->getType()));
        acc->eraseFromParent();
        next->eraseFromParent();
        cur = next_bb;
      }
      llvm::IRBuilder<>(cur).CreateBr(found.Exit);
      found.Exit->replacePhiUsesWith(found.Exiting, cur);
    }

    /// Compute checked running sums exactly, so their loops vectorise
    /// (AIR phase 3, ".triages/air-integer-model.md"). A sum whose
    /// total fits never stops, whatever its partial sums did: a fussy
    /// overflow the spec lets rel finish. Loops whose trip count is
    /// not known to lie between "kExactSumMinBackedges" and
    /// "kExactSumMaxBackedges" are versioned.
    struct ExactSumsPass : llvm::PassInfoMixin<ExactSumsPass> {
      static auto run(llvm::Function &fn, llvm::FunctionAnalysisManager &fam) -> llvm::PreservedAnalyses {
        if (not IsWideEnough(fn, fam)) { return llvm::PreservedAnalyses::all(); }
        auto &li = fam.getResult<llvm::LoopAnalysis>(fn);
        auto &se = fam.getResult<llvm::ScalarEvolutionAnalysis>(fn);
        auto &dt = fam.getResult<llvm::DominatorTreeAnalysis>(fn);
        auto found = Vec<ExactLoop>();
        for (const auto loop : li.getLoopsInPreorder()) {
          if (auto sums = FindExactSums(*loop, se)) { found.emplace_back(std::move(*sums)); }
        }
        auto changed = false;
        for (auto const &loop : found) {
          // Known short: not worth it. Known in range: no guard needed.
          // Otherwise a guard picks, as the count comes out.
          const auto ty = loop.Count->getType();
          const auto max = se.getConstantMaxBackedgeTakenCount(loop.Loop);
          if (const auto c = llvm::dyn_cast<llvm::SCEVConstant>(max); c != nullptr and c->getAPInt().ult(kExactSumMinBackedges)) {
            continue;
          }
          const auto in_range = se.isKnownPredicate(llvm::ICmpInst::ICMP_UGE, loop.Count, se.getConstant(ty, kExactSumMinBackedges))
            and (ty->getIntegerBitWidth() <= 32
              or se.isKnownPredicate(llvm::ICmpInst::ICMP_ULE, loop.Count, se.getConstant(ty, kExactSumMaxBackedges)));
          if (not in_range and not VersionByTripCount(*loop.Loop, loop.Exit, loop.Count, se, li, dt)) { continue; }
          RewriteExactSums(loop, fn);
          changed = true;
        }
        return changed ? llvm::PreservedAnalyses::none() : llvm::PreservedAnalyses::all();
      }
    };

    /// Put each checked op back together for the backend, at the end
    /// of the optimiser. "EmitCheckedArith" returns the plain op,
    /// flagged, so the optimiser can read it, and checks with the
    /// intrinsic's bit; by now InstCombine has often made the check a
    /// comparison and moved the op away from it, and the backend only
    /// fuses an op and its test ("add", "jo") when they are one
    /// intrinsic: otherwise both are emitted (an "add" and a "cmp", or
    /// two multiplies). Each scalar op "r" and an overflow test of it
    /// become one "*.with.overflow" again, its value for "r" and its
    /// bit for the test, placed where it dominates both. Correct
    /// whatever the flags: the intrinsic's value is the op's (wrapped)
    /// result, and its bit the test.
    struct FuseCheckedOpsPass : llvm::PassInfoMixin<FuseCheckedOpsPass> {
      static auto run(llvm::Function &fn, llvm::FunctionAnalysisManager &fam) -> llvm::PreservedAnalyses {
        auto &dt = fam.getResult<llvm::DominatorTreeAnalysis>(fn);
        auto ops = Vec<llvm::BinaryOperator*>();
        for (auto &inst : llvm::instructions(fn)) {
          const auto bo = llvm::dyn_cast<llvm::BinaryOperator>(&inst);
          if (bo != nullptr and bo->getType()->isIntegerTy() and (bo->getOpcode() == llvm::Instruction::Add
            or bo->getOpcode() == llvm::Instruction::Sub or bo->getOpcode() == llvm::Instruction::Mul)) {
            ops.emplace_back(bo);
          }
        }

        auto changed = false;
        for (const auto r : ops) {
          // The candidates: whatever else uses the op's operands.
          auto candidates = Vec<llvm::Instruction*>();
          for (const auto operand : {r->getOperand(0), r->getOperand(1)}) {
            if (llvm::isa<llvm::Constant>(operand)) { continue; }
            for (const auto user : operand->users()) {
              const auto inst = llvm::dyn_cast<llvm::Instruction>(user);
              if (inst != nullptr and inst != r and inst->getFunction() == &fn) { candidates.emplace_back(inst); }
            }
          }
          // An "x u> ~y" test reads "~y", not "y".
          for (const auto user : r->getOperand(1)->users()) {
            const auto inv = llvm::dyn_cast<llvm::Instruction>(user);
            if (inv == nullptr or not llvm::PatternMatch::match(inv, llvm::PatternMatch::m_Not(llvm::PatternMatch::m_Specific(r->getOperand(1))))) { continue; }
            for (const auto u : inv->users()) { if (const auto i = llvm::dyn_cast<llvm::Instruction>(u)) { candidates.emplace_back(i); } }
          }

          for (const auto check : candidates) {
            const auto fused = OverflowCheckOf(r, check);
            if (not fused) { continue; }

            // One intrinsic, where it dominates both.
            const auto at = dt.dominates(check, r) ? check : dt.dominates(r, check) ? static_cast<llvm::Instruction*>(r) : nullptr;
            if (at == nullptr) { continue; }
            auto b = llvm::IRBuilder<>(at);
            const auto w = b.CreateBinaryIntrinsic(fused->Id, r->getOperand(0), fused->Rhs, nullptr, "checked");
            const auto value = b.CreateExtractValue(w, 0, "checked.value");
            const auto bit = b.CreateExtractValue(w, 1, "checked.overflowed");
            if (llvm::isa<llvm::WithOverflowInst>(check)) {
              for (const auto user : Vec<llvm::User*>(check->user_begin(), check->user_end())) {
                const auto ev = llvm::cast<llvm::ExtractValueInst>(user);
                ev->replaceAllUsesWith(ev->getIndices()[0] == 0 ? value : bit);
                ev->eraseFromParent();
              }
            }
            else { check->replaceAllUsesWith(bit); }
            check->eraseFromParent();
            r->replaceAllUsesWith(value);
            r->eraseFromParent();
            changed = true;
            break;
          }
        }
        return changed ? llvm::PreservedAnalyses::none() : llvm::PreservedAnalyses::all();
      }
    };

    /// Defer the overflow checks of loops that can then vectorise. An
    /// s++ check failure aborts the process (nothing unwinds, nothing
    /// can catch it), so aborting at the loop's exit rather than at
    /// the operation cannot be told apart, when the loop does nothing
    /// observable and no wrapped value decides anything before the
    /// exit. "FindDeferredChecks" holds the rules. Every loop is judged
    /// before any is changed, so the analyses describe the function
    /// as it was.
    struct DeferOverflowChecksPass : llvm::PassInfoMixin<DeferOverflowChecksPass> {
      static auto run(llvm::Function &fn, llvm::FunctionAnalysisManager &fam) -> llvm::PreservedAnalyses {
        if (not IsWideEnough(fn, fam)) { return llvm::PreservedAnalyses::all(); }
        auto &se = fam.getResult<llvm::ScalarEvolutionAnalysis>(fn);
        auto &aa = fam.getResult<llvm::AAManager>(fn);
        auto found = Vec<Pair<llvm::Loop*, Vec<Check>>>();
        for (const auto loop : fam.getResult<llvm::LoopAnalysis>(fn).getLoopsInPreorder()) {
          if (auto checks = FindDeferredChecks(*loop, se, aa)) { found.emplace_back(loop, std::move(*checks)); }
        }
        for (auto const &[loop, checks] : found) { DeferChecks(*loop, checks, fn); }
        return found.empty() ? llvm::PreservedAnalyses::all() : llvm::PreservedAnalyses::none();
      }
    };
  }
}

auto spp::codegen::AddCheckedFusePasses(
  llvm::ModulePassManager &mpm) -> void {
  mpm.addPass(llvm::createModuleToFunctionPassAdaptor(FuseCheckedOpsPass()));
}

auto spp::codegen::AddAirPasses(
  llvm::FunctionPassManager &fpm) -> void {
  // Both passes need each loop's preheader and single latch, which
  // loops need not have by now, and the exact sums need closed form
  // (a value leaving the loop only through the exit's phis). The
  // deferral's flags are promoted at once, and the loops put back in
  // the closed form the vectoriser reads.
  fpm.addPass(llvm::LoopSimplifyPass());
  fpm.addPass(llvm::LCSSAPass());
  fpm.addPass(ExactSumsPass());
  fpm.addPass(llvm::LoopSimplifyPass());
  fpm.addPass(DeferOverflowChecksPass());
  fpm.addPass(llvm::PromotePass());
  fpm.addPass(llvm::LCSSAPass());
}
