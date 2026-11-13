module;
#include <spp/macros.hpp>

/// Control-flow integrity (KCFI): every function called indirectly is stamped,
/// as it is emitted, with a hash of the S++ type it is called as ("StampCfi*"),
/// and every indirect call carries the hash it expects ("Cfi*CallBundles"). The
/// module passes turn the stamps into LLVM's "kcfi_type" ("ApplyCfiTypes",
/// "ApplyCfiCoroTypes") and check nothing was missed ("FinishCfi").
export module spp.codegen.llvm_cfi;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, struct TypeRef);

namespace spp::codegen {
  /// Which of a generator's two functions a call through its frame reaches; the
  /// one resuming it, or the one destroying it. Each has its own control-flow
  /// type.
  SPP_EXP_CLS enum class CoroEdge { Resume, Destroy };

  /// Record that "fn" is called indirectly as a value whose function type is
  /// "fn_sup" ("FindFnSup"), which "ApplyCfiTypes" turns into the type a call
  /// checks for. Recorded as "metadata" on the function. The type is a hash of
  /// the function type's "Args" and "Out", the s++ signature, not of which
  /// "FunXXX" it is, as a "FunRef" can be passed as a "FunMov" (constrained
  /// generic).
  SPP_EXP_FUN auto StampCfiFn(
    llvm::Function &fn, TypeRef const &fn_sup) -> void;

  /// Record a coroutine's generator. The lowering splits it into the functions
  /// resuming and destroying it, which keep this, and "ApplyCfiTypes" types
  /// ach of them by which it is.
  SPP_EXP_FUN auto StampCfiCoroutine(
    llvm::Function &ramp, TypeRef const &gen_sup) -> void;

  /// The operand bundle an indirect call through a value whose function type is
  /// "fn_sup" carries, so the backend checks that what it reaches was stamped
  /// with the same type (KCFI).
  SPP_EXP_FUN auto CfiFnCallBundles(
    TypeRef const &fn_sup, LlvmCtx const *ctx) -> Vec<llvm::OperandBundleDef>;

  /// The same for resuming or destroying a generator whose generator type is
  /// "gen_sup" ("FindGenSup"), which the coroutine lowering turns into a call
  /// through its frame.
  SPP_EXP_FUN auto CfiGenCallBundles(
    TypeRef const &gen_sup, CoroEdge edge,
    LlvmCtx const *ctx) -> Vec<llvm::OperandBundleDef>;

  /// Mark "fn" as a closure's capture destructor, the type the call in
  /// "EmitDropClosureEnv" checks it against.
  SPP_EXP_FUN auto StampCfiClosureEnvDrop(llvm::Function &fn) -> void;

  /// The operand bundle the call through a closure's environment header to its
  /// capture destructor carries ("StampCfiClosureEnvDrop").
  SPP_EXP_FUN auto CfiClosureEnvDropCallBundles(
    LlvmCtx const *ctx) -> Vec<llvm::OperandBundleDef>;

  /// The control-flow-integrity type of a function type ("FindFnSup"): a hash
  /// of its "Args" and "Out".
  SPP_EXP_FUN auto CfiFnType(TypeRef const &fn_sup) -> std::uint32_t;

  /// Check every indirect call in a "llvm::Module" against the S++ signature of
  /// what it reaches (KCFI). Each function recorded as callable indirectly
  /// carries its type just before its entry, and each call compares it with the
  /// type it recorded, trapping on a mismatch, so a corrupted function pointer
  /// or coroutine frame reaches only a function of the same type.
  SPP_EXP_FUN auto ApplyCfiTypes(llvm::Module &llvm_mod) -> bool;

  /// The rest of "ApplyCfiTypes": types the functions that coroutine lowering
  /// split each coroutine into (".resume", ".destroy", ".cleanup"), which only
  /// exist part-way through the optimiser. Runs after the split, but before
  /// function merging could fold a part into a thunk that keeps no S++ record.
  SPP_EXP_FUN auto ApplyCfiCoroTypes(llvm::Module &llvm_mod) -> void;

  /// The after-optimiser half of "ApplyCfiTypes". Raises an Ice for any
  /// indirect call left without a "kcfi" bundle, as that call would go
  /// unchecked: error rather than silently fail. Also apply some specific
  /// Mach-O fixes where KCFI type labels get subsumed by the previous function.
  SPP_EXP_FUN auto FinishCfi(llvm::Module &llvm_mod) -> void;

  /// Give every function in a "llvm::Module" a landing pad at its entry, and
  /// every block an indirect jump reaches one too (LLVM places them), and mark
  /// the object as needing none other (BTI). Its GNU property tells the loader
  /// to guard its code, so an indirect branch anywhere else faults. Run last
  /// among the hardening passes, once nothing adds a function. A target that
  /// cannot enforce it falls back on the type checks on indirect calls
  /// ("ApplyCfiTypes", stricter than any landing pad) when they are on, and
  /// otherwise fails, naming the key.
  SPP_EXP_FUN auto ApplyBranchTargets(llvm::Module &llvm_mod) -> bool;

  /// Make every function in a "llvm::Module" that saves its return address
  /// sign it first, with the stack pointer as context, and authenticate it
  /// before returning (pac-ret, the A key), so a return address overwritten on
  /// the stack faults rather than being returned to. Run with the other last
  /// hardening passes. A target that cannot sign falls back on the split stack
  /// ("ApplySafeStack"), which keeps what a callee could overflow away from
  /// return addresses, when it is on, and otherwise fails, naming the key.
  SPP_EXP_FUN auto ApplyReturnSigning(llvm::Module &llvm_mod) -> bool;
}
