module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_support;
import spp.codegen.llvm_target;
import llvm;
import std;

/// What the selected target (and LLVM's backend for it) can do, one check per
/// capability: the questions each hardening measure asks before it is applied,
/// and whose answer decides between the measure, its documented fallback, and a
/// build error naming its key (S0.4). Every such check lives here, and asks it
/// of the selected target unless given another triple (as the tests do).
namespace spp::codegen {
  /// Whether LLVM has somewhere to keep the unsafe stack pointer for the
  /// target, which "[memory.stack] split" (safestack) needs: the runtime's
  /// thread-local, or a libc's accessor. Without it, LLVM reports an error and
  /// emits code that writes through an arbitrary register. Never Windows: sppc
  /// is a DLL there, and a DLL cannot export a thread-local, so the pointer
  /// LLVM names would be an undefined symbol at the link.
  SPP_EXP_FUN auto DoesTargetSupportSafeStack(
    llvm::Triple const &triple = TargetTriple()) -> bool;

  /// Whether LLVM's safestack pass can check a canary for the target, which
  /// "[memory.stack] protect" and "split" together need: the pass checks it
  /// itself, always through "__stack_chk_fail", never a target's own route
  /// (MSVC's "__security_check_cookie"). Without it, LLVM reports an error for
  /// every protected function.
  SPP_EXP_FUN auto DoesTargetSupportSafeStackCanary(
    llvm::Triple const &triple = TargetTriple()) -> bool;

  /// Whether the target probes a large frame inline ("probe-stack" =
  /// "inline-asm"): every target but Windows, which probes every frame larger
  /// than a page already (the ABI's "__chkstk"), and whose backends have no
  /// inline probe; there the attribute's value is taken as the name of a
  /// function to call instead, so "inline-asm" would be an undefined symbol.
  SPP_EXP_FUN auto DoesTargetSupportInlineStackProbes(
    llvm::Triple const &triple = TargetTriple()) -> bool;

  /// Whether the target has straight-line speculation hardening: x86 and
  /// AArch64 do, nothing else does. Not AArch64 Mach-O: LLVM calls its
  /// indirect-call thunks by the unprefixed name, so every call is an undefined
  /// symbol. Todo: drop the Mach-O case once fixed (U16).
  SPP_EXP_FUN auto DoesTargetSupportSlsHardening(
    llvm::Triple const &triple = TargetTriple()) -> bool;

  /// Whether the target can scrub registers on return: x86 and AArch64 can.
  SPP_EXP_FUN auto DoesTargetSupportRegisterScrubbing(
    llvm::Triple const &triple = TargetTriple()) -> bool;

  /// Whether LLVM writes the target's personality and LSDA pointers
  /// PC-relative, as a position-independent executable needs: every target but
  /// m68k, which it leaves absolute (TargetLoweringObjectFileELF has no case
  /// for it).
  SPP_EXP_FUN auto DoesTargetSupportPcRelEhPointers(
    llvm::Triple const &triple = TargetTriple()) -> bool;

  /// Whether the target can check indirect calls by type (KCFI): x86-64 and
  /// AArch64 can.
  SPP_EXP_FUN auto DoesTargetSupportKcfi(
    llvm::Triple const &triple = TargetTriple()) -> bool;

  /// Whether the target can have the processor enforce landing pads on
  /// indirect branches (BTI): AArch64 ELF, which Linux and FreeBSD guard.
  /// Mach-O and COFF on AArch64 do not enforce it.
  SPP_EXP_FUN auto DoesTargetSupportBti(
    llvm::Triple const &triple = TargetTriple()) -> bool;

  /// Whether the target signs return addresses (pac-ret): AArch64 ELF, as for
  /// BTI. Its instructions are hints, so they cost nothing on a processor
  /// without pointer authentication.
  SPP_EXP_FUN auto DoesTargetSupportPacRet(
    llvm::Triple const &triple = TargetTriple()) -> bool;

  /// Whether the target has an indirect jump for a jump table to go through:
  /// every target but WebAssembly, whose switches stay compares.
  SPP_EXP_FUN auto DoesTargetSupportIndirectBranches(
    llvm::Triple const &triple = TargetTriple()) -> bool;
}
