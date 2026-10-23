module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_artefact;
import spp.utils.types;

namespace spp::codegen {
  /// Record in an "llvm::Module" (passed as an opaque pointer)
  /// what the program was built with ("record": one fact per
  /// line), for "spp audit" to read back out of the executable
  /// ("ReadHardeningRecord"). On ELF it is a note, which "strip"
  /// keeps; elsewhere a section of its own. No paths or times
  /// go in, so a build stays reproducible.
  SPP_EXP_FUN auto EmitHardeningRecord(void *llvm_module, char const *record) -> void;

  /// The hardening record of the built program at "path": the
  /// text "EmitHardeningRecord" was given, or none when it has
  /// none (not built by spp, or by one from before records were
  /// written). The error, when the file cannot be read as an
  /// object at all.
  SPP_EXP_FUN auto ReadHardeningRecord(char const *path) -> Expected<Opt<Str>, Str>;

  /// Whether the shared library at "path" was built without
  /// fortified libc calls (_FORTIFY_SOURCE): answers a function
  /// it imports unchecked that libc has a checked "__X_chk" for,
  /// when it imports some and no checked one at all. Which calls
  /// libc can check is read from libc's own exports, not listed
  /// here. None otherwise: fortified, nothing to fortify, or not
  /// an ELF library, which has no such scheme to check for.
  SPP_EXP_FUN auto UnfortifiedImport(char const *path) -> Opt<Str>;

  /// Whether the shared library at "path" was linked without
  /// full RELRO: answers what it lacks ("PT_GNU_RELRO",
  /// "BIND_NOW", or both), as a program linked with
  /// "-z relro -z now" has the one segment made read-only once
  /// relocated and every symbol bound before that. None when it
  /// has both, or is not an ELF library, which has no RELRO.
  SPP_EXP_FUN auto UnhardenedLink(char const *path) -> Opt<Str>;

  /// Whether the program at "path" was laid out without the
  /// segment separation "-z separate-code" and W^X ask for:
  /// answers the first fault found, a segment both writable and
  /// executable or code sharing a file page with anything else
  /// (the ELF header included), as mapping the page executable
  /// makes that executable too. Read from the program headers,
  /// so a linker that ignored the flags is caught. None when the
  /// layout is sound or it is not ELF; the error, when it cannot
  /// be read.
  SPP_EXP_FUN auto UnhardenedSegments(char const *path) -> Expected<Opt<Str>, Str>;
}
