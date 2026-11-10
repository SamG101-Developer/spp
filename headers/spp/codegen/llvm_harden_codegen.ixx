module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_harden_codegen;

namespace spp::codegen {
  /// Marks the declaration of an "!ffi" function, whose calls
  /// "ApplyFfiUnwindGuard" turns into guarded invokes.
  SPP_EXP_FUN inline constexpr char const kFfiFnAttr[] = "spp-ffi";

  /// The control-flow-integrity type a function is called as
  /// ("StampCfiFn").
  SPP_EXP_FUN inline constexpr char const kCfiMd[] = "spp.cfi";

  /// A coroutine's resume and destroy types, which the functions
  /// its lowering splits it into keep ("StampCfiCoroutine").
  SPP_EXP_FUN inline constexpr char const kCfiCoroMd[] = "spp.cfi.coro";

  /// Harden every function in an "llvm::Module" (passed as an
  /// opaque pointer) against straight-line speculation: a trap
  /// straight after every return and indirect jump, so the
  /// bytes that follow one are never run speculatively as the
  /// gadget they would otherwise be. Code size only; the trap
  /// is never reached architecturally. x86 and AArch64 only:
  /// on any other target this reports the key that turns it
  /// off and answers false, which fails the build.
  SPP_EXP_FUN auto ApplySlsHardening(void *llvm_module) -> bool;

  /// Make every function in an "llvm::Module" (passed as an
  /// opaque pointer) zero the general-purpose registers it used
  /// and its caller does not expect kept, just before it
  /// returns, so a pointer or secret it computed is not left
  /// for whatever runs next, and a return-oriented chain finds
  /// none of them preloaded. A few "xor"s on every return.
  /// x86 and AArch64 only: on any other target this reports
  /// the key that turns it off and answers false, which fails
  /// the build.
  SPP_EXP_FUN auto ApplyRegisterScrubbing(void *llvm_module) -> bool;

  /// Check every indirect call in an "llvm::Module" (passed as
  /// an opaque pointer) against the s++ signature of what it
  /// reaches (KCFI): each function recorded as callable
  /// indirectly carries its type just before its entry, and
  /// each call compares it with the type it recorded, trapping
  /// on a mismatch, so a corrupted function pointer or
  /// coroutine frame reaches only a function of the same type.
  /// A load and a compare per call. x86 and AArch64 only: on
  /// any other target this reports the key that turns it off
  /// and answers false, which fails the build.
  ///
  /// This half types the functions, turning the "spp.cfi"
  /// records into llvm's "kcfi_type", and must run before the
  /// optimiser: llvm's passes keep "kcfi_type" on what they
  /// rebuild (a MergeFunctions thunk, a clone), but not s++'s
  /// own metadata, so a function rebuilt before typing would be
  /// left untyped and every indirect call to it would trap.
  SPP_EXP_FUN auto ApplyCfiTypes(void *llvm_module) -> bool;

  /// The rest of "ApplyCfiTypes": types the functions coroutine
  /// lowering split each coroutine into (".resume", ".destroy",
  /// ".cleanup"), which only exist part-way through the optimiser.
  /// Runs there, as "RunOptimizationPipeline"'s "after_coro_split"
  /// hook: after the split, and before MergeFunctions could fold a
  /// part into a thunk that keeps no s++ record.
  SPP_EXP_FUN auto ApplyCfiCoroTypes(void *llvm_module) -> void;

  /// The other half of "ApplyCfiTypes", run after the optimiser:
  /// an Ice for any indirect call left without a "kcfi" bundle,
  /// as that call would go unchecked.
  SPP_EXP_FUN auto CheckCfiCalls(void *llvm_module) -> void;

  /// Keeps each typed function beside its type on Mach-O, run after
  /// the optimiser. LLVM writes a function's KCFI type as a bare word
  /// before its label, which Mach-O gives to the function before it;
  /// ld64 moves a "cold" function to the end of the text, leaving the
  /// word behind, so every indirect call to it traps. A typed function
  /// is kept off "cold" there. Nothing to do for any other format.
  SPP_EXP_FUN auto KeepCfiTypesAttached(void *llvm_module) -> void;

  /// Keep every switch in an "llvm::Module" (passed as an opaque
  /// pointer) from jumping through an entry past the end of its
  /// jump table when the bounds check before it is mispredicted.
  /// On AArch64 the table stays and its index is clamped without
  /// a branch; x86 has no such lowering, so its switches are
  /// built without tables, as compares, which also leaves no
  /// indirect jump for anything to steer. A few instructions per
  /// switch. On any other target this reports the key that turns
  /// it off and answers false, which fails the build.
  SPP_EXP_FUN auto ApplyJumpTableBounds(void *llvm_module) -> bool;

  /// Make every call to an "!ffi" function in an "llvm::Module"
  /// (passed as an opaque pointer) an invoke whose unwind edge
  /// fails the "unwound into s++" check, naming the function.
  /// Foreign code can throw (a C++ library), and an exception
  /// a C++ frame further out catches would otherwise unwind
  /// through the s++ frames between, running none of their
  /// defers and consuming none of their linear values. Free on
  /// the path that returns: the edge is unwind-table data. On
  /// m68k, whose unwind tables LLVM cannot write position-
  /// independently, this reports the key that turns it off and
  /// answers false, which fails the build.
  SPP_EXP_FUN auto ApplyFfiUnwindGuard(void *llvm_module) -> bool;

  /// Give every function in an "llvm::Module" (passed as an
  /// opaque pointer) asynchronous unwind tables, so a stack can
  /// be walked from any instruction in it. Nothing here throws,
  /// so without them every function is "nounwind" with no
  /// ".eh_frame" entry, and the runtime's crash handler cannot
  /// see past the first s++ frame. The cost is binary size
  /// only. Answers how many functions were given one.
  SPP_EXP_FUN auto ApplyUnwindTables(void *llvm_module) -> unsigned long;
}
