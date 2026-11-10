module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_harden_codegen;

namespace spp::codegen {
  /// Marks the declaration of an "!ffi" function, whose calls are turned into
  /// guarded invokes by the "apply ffi unwind guard" logic.
  SPP_EXP_FUN inline constexpr char kFfiFnAttr[] = "spp-ffi";

  /// The control-flow-integrity type a function is called as ("StampCfiFn").
  SPP_EXP_FUN inline constexpr char kCfiMd[] = "spp.cfi";

  /// A coroutine's resume and destroy types, which the functions its lowering
  /// splits it into keep ("StampCfiCoroutine").
  SPP_EXP_FUN inline constexpr char kCfiCoroMd[] = "spp.cfi.coro";

  /// Harden every function in a "llvm::Module" against straight-line
  /// speculation: a trap straight after every return and indirect jump, so the
  /// bytes that follow one are never run speculatively as the gadget they would
  /// otherwise be.
  SPP_EXP_FUN auto ApplySlsHardening(void *llvm_module) -> bool;

  /// Make every function in a "llvm::Module" zero the general-purpose registers
  /// it used and its caller does not expect kept, just before it returns. This
  /// prevents a pointer or secret it computed form being left for whatever runs
  /// next, and means that a return-oriented chain finds none of them preloaded.
  SPP_EXP_FUN auto ApplyRegisterScrubbing(void *llvm_module) -> bool;

  /// Check every indirect call in a "llvm::Module" against the S++ signature of
  /// what it reaches (KCFI). Each function recorded as callable indirectly
  /// carries its type just before its entry, and each call compares it with the
  /// type it recorded, trapping on a mismatch, so a corrupted function pointer
  /// or coroutine frame reaches only a function of the same type.
  SPP_EXP_FUN auto ApplyCfiTypes(void *llvm_module) -> bool;

  /// The rest of "ApplyCfiTypes": types the functions that coroutine lowering
  /// split each coroutine into (".resume", ".destroy", ".cleanup"), which only
  /// exist part-way through the optimiser. Runs after the split, but before
  /// function merging could fold a part into a thunk that keeps no S++ record.
  SPP_EXP_FUN auto ApplyCfiCoroTypes(void *llvm_module) -> void;

  /// The after-optimiser half of "ApplyCfiTypes". Raises an Ice for any
  /// indirect call left without a "kcfi" bundle, as that call would go
  /// unchecked: error rather than silently fail. Also apply some specific
  /// Mach-O fixes where KCFI type labels get subsumed by the previous function.
  SPP_EXP_FUN auto FinishCfi(void *llvm_module) -> void;

  /// Make every call to an "!ffi" function in a "llvm::Module" an invoke whose
  /// unwind edge fails the "unwound into S++" check, naming the function.
  /// Foreign code can throw (a C++ library), and an exception a C++ frame
  /// further out catches would otherwise unwind through the S++ frames between,
  /// running none of the defers and consuming none of their linear values. This
  /// is zero-cost on the path that returns: the edge is unwind-table data.
  SPP_EXP_FUN auto ApplyFfiUnwindGuard(void *llvm_module) -> bool;

  /// Give every function in a "llvm::Module" asynchronous unwind tables, so a
  /// stack can be walked from any instruction in it. Nothing here throws, so
  /// without them every function is "nounwind" with no ".eh_frame" entry, and
  /// the runtime's crash handler cannot see past the first S++ frame. The cost
  /// is binary size only. Returns how many functions were given one.
  SPP_EXP_FUN auto ApplyUnwindTables(void *llvm_module) -> unsigned long;
}
