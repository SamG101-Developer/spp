module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_artefact;
import spp.utils.types;

/// Handle all the LLVM logic related to the created artefact, including
/// hardening records, fortification checks, link checks, and segment checks.
/// Some atomic checks are in place too, to prevent undefined symbol usage.
namespace spp::codegen {
  /// Record in an "llvm::Module" (passed as an opaque pointer) what the program
  /// was built with ("record": one fact per line), for "spp audit" to read back
  /// out of the executable.
  SPP_EXP_FUN auto EmitHardeningRecord(
    void *llvm_module, char const *record) -> void;

  /// Get the hardening record placed inside the binary. It is optional, because
  /// we might have a valid binary with no record, and the entire optional part
  /// is wrapped in expected, in the case that the binary can't be read at all.
  SPP_EXP_FUN auto ReadHardeningRecord(
    char const *exe_path) -> Expected<Opt<Str>, Str>;

  /// Check if the shared library specified by "path" was built without
  /// fortified libc calls, (specified by _FORTIFY_SOURCE). If there are any
  /// unfortified calls, then return them in the string, otherwise None for
  /// fully fortified, nothing to fortify, or non-ELF.
  SPP_EXP_FUN auto UnfortifiedImport(char const *path) -> Opt<Str>;

  /// Check if the shared library specified by "path" was built without full
  /// RELRO (relocation read-only). The returned string contains anything part
  /// of RELRO that it lacks, otherwise None when full RELRO is available or
  /// non-ELF.
  SPP_EXP_FUN auto UnhardenedLink(char const *path) -> Opt<Str>;

  /// Check if the shared library specified by "path" was built without hardened
  /// segment separation. The returned string contains anything missing, such as
  /// W&X segments, or code sharing file pages with anything else, otherwise
  /// None when the layout is sound or non-ELF, or the unexpected error when it
  /// cannot be read.
  SPP_EXP_FUN auto UnhardenedSegments(
    char const *path) -> Expected<Opt<Str>, Str>;

  /// Check if the object specified by "path" calls into the "libatomic" library
  /// to find if there are any undefined "__atomic_*" symbols, which LLVM might
  /// leave for an atomic that its target has no instructions for (this is seen
  /// on m68k and the wide ones on most 32-bit targets).
  SPP_EXP_FUN auto ImportsAtomicLibcalls(char const *path) -> bool;
}
