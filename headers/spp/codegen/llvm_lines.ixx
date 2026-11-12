module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_lines;
import spp.utils.types;
import llvm;
import std;

/// The source-line table a program carries for its crash reports (sppc's
/// "SOURCE LINES", backtrace/common.h). It is built from the debug info in the
/// program's object, so DWARF stays the one record of where code came from,
/// and linked in as a second object holding only data. Each function is keyed
/// by its symbol's name, and each place by its offset into the function, so
/// the table needs no relocations. The S++ functions are never named from IR:
/// taking their addresses would give each one a BTI landing pad. "main" hands
/// it to sppc before the program starts, and sppc reads it without parsing any
/// object format, so it reads the same on every platform.
///
/// Functions related to the "line table", a source line table carried for crash
/// reports (aborts, failed checks, etc). The line table is built from the debug
/// info in the program's object (DWARF is the single source of truth)
namespace spp::codegen {
  /// The table for the functions in the object at "object_path", as bytes in
  /// the object's byte order. A file under "root" (the project) is named
  /// relative to it. "global_prefix" is the character the target puts before
  /// every symbol ('_' on Mach-O), dropped as sppc's lookup drops it; '\0' for
  /// none. Nothing if the object cannot be read.
  SPP_EXP_FUN auto BuildLineTable(
    Path const &object_path, Path const &root,
    char global_prefix) -> Opt<Vec<std::uint8_t>>;

  /// Write "table" to an object at "path", defining the symbol "main" hands to
  /// sppc. The object uses "program"'s target and module flags, because the
  /// linker ANDs properties such as BTI across every object it is given.
  SPP_EXP_FUN auto EmitLineTableObject(
    llvm::Module const &program, Vec<std::uint8_t> const &table,
    Path const &path, unsigned opt_level) -> bool;

  /// Make "program"'s C entry point hand sppc the table before anything else,
  /// so a report from any later point has it. The table itself is linked in
  /// from the object "EmitLineTableObject" writes.
  SPP_EXP_FUN auto RegisterLineTable(llvm::Module &program) -> void;
}
