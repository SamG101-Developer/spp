module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_artefact;

namespace spp::codegen {
  /// Record in an "llvm::Module" (passed as an opaque pointer)
  /// what the program was built with ("record": one fact per
  /// line), for "spp audit" to read back out of the executable
  /// ("ReadHardeningRecord"). On ELF it is a note, which "strip"
  /// keeps; elsewhere a section of its own. No paths or times
  /// go in, so a build stays reproducible.
  SPP_EXP_FUN auto EmitHardeningRecord(void *llvm_module, char const *record) -> void;

  /// The hardening record of the built program at "path": the
  /// text "EmitHardeningRecord" was given, or empty when it has
  /// none (not built by spp, or by one from before records were
  /// written). Null, with "error" set, when the file cannot be
  /// read as an object at all. Both strings are valid until the
  /// next call.
  SPP_EXP_FUN auto ReadHardeningRecord(char const *path, char const **error) -> char const*;

  /// Whether the shared library at "path" was built without
  /// fortified libc calls (_FORTIFY_SOURCE): answers a function
  /// it imports unchecked that libc has a checked "__X_chk" for,
  /// when it imports some and no checked one at all. Which calls
  /// libc can check is read from libc's own exports, not listed
  /// here. Null otherwise: fortified, nothing to fortify, or not
  /// an ELF library, which has no such scheme to check for.
  /// Valid until the next call.
  SPP_EXP_FUN auto UnfortifiedImport(char const *path) -> char const*;
}
