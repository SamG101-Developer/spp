module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_harden_codegen;
import llvm;

namespace spp::codegen {
  /// Marks the declaration of an "!ffi" function, whose calls are turned into
  /// guarded invokes by the "apply ffi unwind guard" logic.
  SPP_EXP_FUN inline constexpr char kFfiFnAttr[] = "spp-ffi";

  /// Harden every function in a "llvm::Module" against straight-line
  /// speculation: a trap straight after every return and indirect jump, so the
  /// bytes that follow one are never run speculatively as the gadget they would
  /// otherwise be.
  SPP_EXP_FUN auto ApplySlsHardening(llvm::Module &llvm_mod) -> bool;

  /// Make every function in a "llvm::Module" zero the general-purpose registers
  /// it used and its caller does not expect kept, just before it returns. This
  /// prevents a pointer or secret it computed form being left for whatever runs
  /// next, and means that a return-oriented chain finds none of them preloaded.
  SPP_EXP_FUN auto ApplyRegisterScrubbing(llvm::Module &llvm_mod) -> bool;

  /// Make every call to an "!ffi" function in a "llvm::Module" an invoke whose
  /// unwind edge fails the "unwound into S++" check, naming the function.
  /// Foreign code can throw (a C++ library), and an exception a C++ frame
  /// further out catches would otherwise unwind through the S++ frames between,
  /// running none of the defers and consuming none of their linear values. This
  /// is zero-cost on the path that returns: the edge is unwind-table data.
  SPP_EXP_FUN auto ApplyFfiUnwindGuard(llvm::Module &llvm_mod) -> bool;

  /// Give every function in a "llvm::Module" asynchronous unwind tables, so a
  /// stack can be walked from any instruction in it. Nothing here throws, so
  /// without them every function is "nounwind" with no ".eh_frame" entry, and
  /// the runtime's crash handler cannot see past the first S++ frame. The cost
  /// is binary size only. Returns how many functions were given one.
  SPP_EXP_FUN auto ApplyUnwindTables(llvm::Module &llvm_mod) -> unsigned long;
}
