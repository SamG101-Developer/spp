module;
#include <spp/macros.hpp>

export module spp.compiler.module_tree;
import spp.compiler.out_layout;
import spp.lex.tokens;
import spp.utils.error_formatter;
import spp.utils.files;
import spp.utils.types;
import genex;
import std;

namespace spp::asts {
  SPP_EXP_CLS struct ModulePrototypeAst;
}

namespace spp::compiler {
  SPP_EXP_CLS struct Module;
  SPP_EXP_CLS struct ModuleTree;
  SPP_EXP_CLS struct TestScope;
}

/// Which "tst" folders a build picks up. Empty means an ordinary build, which
/// compiles none of them. The "spp test" flags can limit to specific libraries
/// or include all of them etc.
SPP_EXP_CLS struct spp::compiler::TestScope {
  /// Include the Project's own "tst" folder.
  bool Project = false;

  /// Include the "tst" folder of every "vcs" library.
  bool AllLibs = false;

  /// Include the "tst" folder of these specific libraries.
  Vec<Str> Libs;

  /// Check if any test folder has been chosen to run. This will be false for a
  /// normal build/run. Only "spp test" can fill this.
  SPP_ATTR_NODISCARD auto Any() const -> bool {
    return Project or AllLibs or not Libs.empty();
  }

  /// Check if a specific library is to be tested; either it is contained in the
  /// specific library list, or all libraries are being tested.
  SPP_ATTR_NODISCARD auto WantsLib(Str const &lib) const -> bool {
    return AllLibs or genex::contains(Libs, lib);
  }
};

SPP_EXP_CLS struct spp::compiler::Module {
  spp::Path Path = "";
  Str Code;
  Vec<lex::RawToken> Tokens = {};
  Unique<asts::ModulePrototypeAst> ModuleAst;
  Shared<utils::errors::ErrorFormatter> Formatter;

  /// Whether this is the entry point the test build generates. Its source is
  /// not read from disk, it is written once the other modules have been parsed
  /// and the unit tests among them are known, which is why it is parsed after
  /// the rest.
  bool IsTestHarness = false;

  /// The namespace this module's contents live in, as the parts of a "::"
  /// chain. Worked out once, by the tree, which is the only thing that knows
  /// where the real source roots are - the path alone cannot say, because a
  /// module is free to have directories of its own named "src" or "tst".
  Vec<Str> NsParts;

  /// Who shipped this module: the "vcs" dependency it was read from, by its
  /// folder name, or none for the project's own (its "src", "tst" and "ffi",
  /// and the test harness). An ffi package can only be named by its owner's
  /// modules.
  Opt<Str> Owner;

  /// The dependencies its owner's "spp.toml" declares. A module can only name
  /// its own package's namespaces and these.
  Vec<Str> Declares;

  /// Whether this module is a stub of an ffi package, from its owner's "ffi"
  /// folder.
  bool IsFfi = false;

  /// Whether this module is one of the standard library's own sources, from
  /// "vcs/std/src". These import what they use themselves, so the prelude is
  /// not appended to them.
  bool IsStd = false;

  Module(
    spp::Path path, Str code, Vec<lex::RawToken> tokens,
    Unique<asts::ModulePrototypeAst> module_ast,
    Shared<utils::errors::ErrorFormatter> error_formatter);

  /// Build a module from a path. This doesn't fill its information in, so will
  /// be done externally after this method is called.
  static auto FromPath(spp::Path const &path);

  /// The empty entry-point module a test build fills in later. Reserves the
  /// "tst/main.spp" name, so a real file there is not globbed - the harness
  /// owns that namespace.
  static auto TestHarness(spp::Path const &tst_root) -> Unique<Module>;
};

SPP_EXP_CLS struct spp::compiler::ModuleTree {
private:
  /// The root path to the module being compiled. This is the path that owns the
  /// "src" folder and "main.spp".
  Path _Root;

  /// The layout that includes a few paths and files, and can produce all
  /// derived paths as a single source of truth.
  OutLayout _Out;

  /// Cached path for the source files, that are part of this project.
  Path _SrcPath;

  /// Cached path for the vcs files, that are part of this project, and
  /// specified in the toml files [vcs] section.
  Path _VcsPath;

  /// Cached path for the ffi files, that are part of this project, or
  /// dependencies.
  Path _FfiPath;

  /// Cached path for the test files, which form the S++ unit test suite for
  /// this project.
  Path _TstPath;

  /// The list of modules that are being compiled. Each module contains its path
  /// and code, and receives tokens (lexing) and a module ast (parsing).
  Vec<Unique<Module>> _Modules;

  /// A lock that is acquired before all the modules are read, and released
  /// after the code has been cached into storage (for lexing). Allows for
  /// thread safety, for example with the parallel C++ unit test runners.
  utils::files::FileLock _Lock;

  /// Acquire the lock.
  auto Lock() -> void;

  /// Release the lock.
  auto Unlock() -> void;

public:
  /// Create a module tree with a root path, a build mode, and an optional test
  /// scope. The mode is needed for the output path folder name.
  explicit ModuleTree(
    Path path, BuildMode mode = BuildMode::DEV, TestScope const &tests = {});

  /// Create a module tree for a C++ unit test, with the source of the project's
  /// "src/main.spp" replaced by "main_code". That file has to exist.
  static auto ForCppGoogleTest(
    Path path, BuildMode mode, Str &&main_code) -> Unique<ModuleTree>;

  /// Provide a "begin" to allow for simple iteration.
  auto begin() -> Vec<Unique<Module>>::iterator;

  /// Provide an "end" to allow for simple iteration.
  auto end() -> Vec<Unique<Module>>::iterator;

  /// Get the modules as a view over the raw pointers (this tree owns the
  /// modules as unique pointers)
  auto GetModules() -> Vec<Module*>;

  /// Getter for the root path.
  auto RootPath() const -> Path;

  /// Check every top-level namespace belongs to one package, and is not both an
  /// ffi package and a source folder. A namespace is what names and owns its
  /// modules, so two packages sharing one could not be told apart.
  SPP_ATTR_NODISCARD auto CheckNamespaces() const -> bool;

  /// Getter for where this build writes ("OutLayout"). Everything downstream of
  /// code generation asks this rather than rebuilding the path from the root,
  /// so the tree's shape is described in one place.
  SPP_ATTR_NODISCARD auto Out() const -> OutLayout const&;

  /// Where a module's generated LLVM IR belongs: the module's own path mirrored
  /// under the build's llvm tree, with a ".ll" extension.
  auto LlvmOutPathFor(Path const &module_path) const -> Path;

private:
  /// Every directory a module's namespace can be measured from: the project's
  /// "src" and "tst", and the same pair for each library under "vcs". Built in
  /// the constructor and used only by "NamespaceOf".
  Vec<Path> _SourceRoots;

  /// The namespace a module's path puts it in. Anchored on the source root that
  /// actually produced the module - the longest one it sits under - rather than
  /// on the first path component spelled "src" or "tst", so a module with a
  /// directory of its own by either name is not cut at the wrong place. A
  /// module under no source root at all (an ffi stub) is namespaced by the
  /// folder holding it, which is the package it belongs to.
  SPP_ATTR_NODISCARD auto NamespaceOf(
    Path const &module_path) const -> Vec<Str>;

  /// The owner of the module at a path: the folder under "vcs" it sits in, if
  /// any.
  SPP_ATTR_NODISCARD auto OwnerOf(Path const &module_path) const -> Opt<Str>;
};
