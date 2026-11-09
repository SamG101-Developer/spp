module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_entry;

namespace spp::codegen {
  /// Tell LLVM what sppc's allocation functions are ("sppc_malloc",
  /// "sppc_calloc", "sppc_aligned_alloc", "sppc_realloc", "sppc_free",
  /// thin wrappers over the C library's), on every declaration of
  /// them in "llvm_module" (an opaque pointer): their kind, their one
  /// family, which argument is the size, alignment or pointer, and
  /// that a new allocation aliases nothing. With that LLVM treats
  /// them as "malloc" and "free": a pair whose memory nothing else
  /// sees is removed, and alias analysis knows fresh memory is
  /// fresh.
  SPP_EXP_FUN auto ApplyRuntimeAllocatorAttrs(void *llvm_module) -> void;

  /// Add a C "main" to the combined "llvm::Module" (passed as
  /// an opaque pointer) that calls the s++ entry point named
  /// "spp_main_name", so the program has an entry point of the
  /// shape a linker and a libc start-up expect. The s++ entry
  /// point cannot be it directly: its name is mangled, and it
  /// takes its arguments as an s++ "Vec[Str]" rather than as
  /// "(argc, argv)".
  ///
  /// "split_stacks" brings the main thread's unsafe stack up as
  /// part of the runtime start-up. It must match what
  /// "ApplySafeStack" is asked to do later: a program whose
  /// functions read an unsafe stack pointer that was never set
  /// up writes its first split frame through a null one.
  ///
  /// "deny_wx" denies the process writable-and-executable
  /// memory as the last start-up step (MDWE, Linux 6.6+); a
  /// kernel without it fails the program at start-up.
  ///
  /// "seal" seals every loaded object's code, constants and
  /// relocated data after that (mseal, Linux 6.10+), with the
  /// same failure on an older kernel.
  SPP_EXP_FUN auto EmitCEntryPoint(
    void *llvm_module, char const *spp_main_name, bool split_stacks, bool deny_wx, bool seal) -> void;
}
