module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_entry;

namespace spp::codegen {
  /// Tell LLVM what the names of the allocation functions in the sppc library.
  /// These are "sppc_malloc", "sppc_calloc", "sppc_aligned_alloc",
  /// "sppc_realloc" and "sppc_free"; thin wrappers over the C library's). For
  /// each declaration, LLVM can apply attributes for their kind, their one
  /// family, which argument is the size, alignment or pointer, and that a new
  /// allocation aliases nothing.
  SPP_EXP_FUN auto ApplyRuntimeAllocatorAttrs(void *llvm_module) -> void;

  /// Add a C "main" to the combined "llvm::Module", that calls the S++ entry
  /// point named "spp_main_name", so the program has an entry point of the
  /// shape that the linker and libc start-up expect. The S++ entry point cannot
  /// be it directly: its name is mangled, and S++ "main" doesn't take the argv/
  /// argc pair. They go to sppc instead ("sppc_set_args"), first, where
  /// "std::process::args" reads them.
  SPP_EXP_FUN auto EmitCEntryPoint(
    void *llvm_module, char const *spp_main_name, bool split_stacks,
    bool deny_wx, bool seal) -> void;
}
