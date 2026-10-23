module;
#include <spp/macros.hpp>

export module spp.compiler.ffi;
import spp.utils.types;
import std;

namespace spp::compiler::ffi {
  /// Every package the project at "root" can use: its own, then
  /// each dependency's, in a fixed order. Assumes "Validate" has
  /// passed, so no two share a name.
  SPP_EXP_FUN auto Packages(Path const &root) -> Vec<Path>;

  /// Check every package is laid out as the compiler reads it,
  /// for every target it ships rather than only this host's, and
  /// that no two packages share a name - a stub's namespace is
  /// its folder's name, so two would be one namespace defined
  /// twice. Reports the first problem and answers false.
  SPP_EXP_FUN auto Validate(Path const &root) -> bool;

  /// Each package's library for the selected target, or nothing
  /// when a package has none - reported, rather than a link that
  /// quietly leaves it out.
  SPP_EXP_FUN auto Libraries(Path const &root) -> Opt<Vec<Path>>;

  /// Check every library "Libraries" answered was built with
  /// fortified libc calls ("codegen::UnfortifiedImport"),
  /// reporting the first that was not, with what it calls
  /// unchecked and how to fix it, and answering false.
  SPP_EXP_FUN auto CheckFortified(Path const &root, Vec<Path> const &libraries) -> bool;

  /// Check every library "Libraries" answered was linked with
  /// full RELRO ("codegen::UnhardenedLink"), reporting the first
  /// that was not, with what it lacks and how to fix it, and
  /// answering false.
  SPP_EXP_FUN auto CheckRelro(Path const &root, Vec<Path> const &libraries) -> bool;

  /// Check every library "Libraries" answered keeps code apart
  /// from data and has no writable-and-executable segment
  /// ("codegen::UnhardenedSegments"), reporting the first that
  /// does not, or cannot be read, and answering false.
  SPP_EXP_FUN auto CheckSegments(Path const &root, Vec<Path> const &libraries) -> bool;
}
