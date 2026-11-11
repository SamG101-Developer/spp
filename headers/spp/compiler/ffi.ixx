module;
#include <spp/macros.hpp>

export module spp.compiler.ffi;
import spp.utils.types;
import std;

namespace spp::compiler::ffi {
  /// Every package the project at "root" can use: its own, then each
  /// dependency's, in a fixed order. Assumes "Validate" has passed, so no two
  /// share a name.
  SPP_EXP_FUN auto Packages(Path const &root) -> Vec<Path>;

  /// Check every package is laid out as the compiler reads it, for every target
  /// it ships rather than only this host's, and that no two packages share a
  /// name - a stub's namespace is its folder's name, so two would be one
  /// namespace defined twice. Reports the first problem and answers false.
  SPP_EXP_FUN auto Validate(Path const &root) -> bool;

  /// Each package's library for the selected target, or nothing when a package
  /// has none - reported, rather than a link that quietly leaves it out. An ELF
  /// library may carry its version ("libssl.so.4"), and keeps it when staged.
  SPP_EXP_FUN auto Libraries(Path const &root) -> Opt<Vec<Path>>;

  /// The import library a library from "Libraries()" is linked through, beside
  /// it under the same name ("sppc.lib" for "sppc.dll"), on a target that links
  /// one (an MSVC target); nothing where the library itself is linked. On such
  /// a target "Validate" has checked it is there.
  SPP_EXP_FUN auto ImportLibraryOf(Path const &library) -> Opt<Path>;

  /// Check every library from "Libraries()" was built with fortified libc
  /// calls, reporting the first that was not, with what it calls unchecked and
  /// how to fix it, and answering false.
  SPP_EXP_FUN auto CheckFortified(
    Path const &root, Vec<Path> const &libraries) -> bool;

  /// Check every library from "Libraries()" was linked with full RELRO,
  /// reporting the first that was not, with what it lacks and how to fix it,
  /// and answering false.
  SPP_EXP_FUN auto CheckRelro(
    Path const &root, Vec<Path> const &libraries) -> bool;

  /// Check every library from "Libraries()" keeps code apart from data and has
  /// no writable-and-executable segment, reporting the first that does not, or
  /// cannot be read, and answering false.
  SPP_EXP_FUN auto CheckSegments(
    Path const &root, Vec<Path> const &libraries) -> bool;
}
