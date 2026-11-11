module;
#include <spp/macros.hpp>

export module spp.compiler.dependencies;
import spp.utils.types;
import std;

namespace spp::compiler::dependencies {
  SPP_EXP_CLS struct Dependency;

  /// What the "spp.toml" at "root" itself declares in its "[vcs]" table, sorted
  /// by name. None when there is no "spp.toml". Reports each malformed record
  /// and answers nothing. Reads no other file, so it is safe before a fetch.
  SPP_EXP_FUN auto Declared(Path const &root) -> Opt<Vec<Dependency>>;

  /// Every fetched dependency of the project at "root", and of its
  /// dependencies, flat under "<root>/vcs" and sorted by name. This is what the
  /// rest of the compiler reads: a folder under "vcs" that nothing declares is
  /// not a dependency. Assumes "Validate" has passed.
  SPP_EXP_FUN auto Resolve(Path const &root) -> Vec<Dependency>;

  /// Hand every dependency of the project at "root" to "fetch_one" once,
  /// reading what each declares once it has been fetched. Reports a dependency
  /// declared two ways or a malformed record, and answers false, as it does if
  /// any "fetch_one" did. The git work is the caller's.
  SPP_EXP_FUN auto Fetch(Path const &root, FuncRef<bool(Dependency const&)> fetch_one) -> bool;

  /// Check every record is well formed, no dependency is declared two ways,
  /// every dependency has been fetched (a pinned one at its "rev"), and every
  /// package declares "std" (or is it). Reports what is wrong and answers
  /// false.
  SPP_EXP_FUN auto Validate(Path const &root) -> bool;

  /// The commit a checkout at "folder" is detached at, in lower case, read from
  /// its ".git/HEAD" with no git call. None when it is on a branch, or is not a
  /// checkout of its own.
  SPP_EXP_FUN auto FindCheckedOutRev(Path const &folder) -> Opt<Str>;
}

/// One record of a "[vcs]" table.
SPP_EXP_CLS struct spp::compiler::dependencies::Dependency {
  /// The record's key, which is also its folder's name under "vcs".
  Str Name;

  /// Where it is fetched from.
  Str Git;

  /// The branch or tag it is fetched at. Empty when it is pinned to "Rev".
  Str Branch;

  /// The commit it is pinned to, a full hash in lower case. A pinned
  /// dependency is checked out at exactly this commit and never pulled.
  Opt<Str> Rev;

  /// Where it is fetched to: "<root>/vcs/<name>".
  Path Folder;
};
