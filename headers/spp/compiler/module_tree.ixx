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

/**
 * Which "tst" folders a build picks up. Empty means an ordinary build, which compiles none of them.
 */
SPP_EXP_CLS struct spp::compiler::TestScope {
  /** Include the project's own "tst" folder. */
  bool project = false;

  /** Include the "tst" folder of every library under "vcs". */
  bool all_libs = false;

  /** Include the "tst" folder of these libraries, named as their folder under "vcs". */
  Vec<Str> libs;

  SPP_ATTR_NODISCARD auto Any() const -> bool { return project or all_libs or not libs.IsEmpty(); }

  SPP_ATTR_NODISCARD auto WantsLib(Str const &lib) const -> bool {
    return all_libs or genex::contains(libs, lib);
  }
};

SPP_EXP_CLS struct spp::compiler::Module {
  std::filesystem::path path = "";
  Str code;
  Vec<lex::RawToken> tokens = {};
  Unique<asts::ModulePrototypeAst> module_ast;
  Shared<utils::errors::ErrorFormatter> error_formatter;

  /**
   * Whether this is the entry point the test build generates. Its source is not read from disk: it is written once the
   * other modules have been parsed and the unit tests among them are known, which is why it is parsed after the rest.
   */
  bool is_test_harness = false;

  /**
   * The namespace this module's contents live in, as the parts of a "::" chain. Worked out once, by the tree, which is
   * the only thing that knows where the real source roots are - the path alone cannot say, because a module is free to
   * have directories of its own named "src" or "tst".
   */
  Vec<Str> ns_parts;

  Module(
    std::filesystem::path path,
    Str code,
    Vec<lex::RawToken> tokens,
    Unique<asts::ModulePrototypeAst> module_ast,
    Shared<utils::errors::ErrorFormatter> error_formatter);

  static auto FromPath(
    std::filesystem::path const &path);

  /**
   * The empty entry-point module a test build fills in later. Reserves the "tst/main.spp" name, so a real file there
   * is not globbed - the harness owns that namespace.
   */
  static auto TestHarness(
    std::filesystem::path const &tst_root)
    -> Unique<Module>;
};

SPP_EXP_CLS struct spp::compiler::ModuleTree {
private:
  std::filesystem::path m_root;
  OutLayout m_out;
  std::filesystem::path m_src_path;
  std::filesystem::path m_vcs_path;
  std::filesystem::path m_ffi_path;
  std::filesystem::path m_tst_path;
  Vec<Unique<Module>> m_modules;
  utils::files::FileLock m_lock;

  auto Lock()
    -> void;

  auto Unlock()
    -> void;

public:
  /**
   * @param[in] path The project root.
   * @param[in] mode The build mode, "dev" or "rel". Only the out tree depends on it; which modules are picked up does
   * not, so this is carried rather than acted on until code generation writes something.
   */
  explicit ModuleTree(
    std::filesystem::path path,
    Str mode = "dev",
    TestScope const &tests = {});

  static auto ForCppGoogleTest(
    std::filesystem::path path,
    Str mode,
    Str &&main_code)
    -> Unique<ModuleTree>;

  auto begin()
    -> Vec<Unique<Module>>::iterator;

  auto end()
    -> Vec<Unique<Module>>::iterator;

  auto GetModules()
    -> Vec<Module*>;

  auto RootPath() const
    -> std::filesystem::path;

  /**
   * Where this build writes: see @c OutLayout . Everything downstream of code generation asks this rather than
   * rebuilding the path from the root, so the tree's shape is described in one place.
   */
  SPP_ATTR_NODISCARD auto Out() const
    -> OutLayout const&;

  /**
   * Where a module's generated LLVM IR belongs: the module's own path mirrored under the build's @c llvm tree,
   * with a @c .ll extension. The mirror is anchored on the source root that actually produced the module rather than
   * on the first path component spelled @c src , because a module is free to have directories of its own by that
   * name and only the real root decides where the mirrored part starts.
   * @param[in] module_path The absolute path of the module's source file.
   * @return The absolute path to write its IR to. Its parent directory is not created.
   */
  auto LlvmOutPathFor(
    std::filesystem::path const &module_path) const
    -> std::filesystem::path;

private:
  /**
   * Every directory a module's namespace can be measured from: the project's "src" and "tst", and the same pair for
   * each library under "vcs". Built in the constructor and used only by @c NamespaceOf .
   */
  Vec<std::filesystem::path> m_source_roots;

  /**
   * The namespace a module's path puts it in. Anchored on the source root that actually produced the module - the
   * longest one it sits under - rather than on the first path component spelled "src" or "tst", so a module with a
   * directory of its own by either name is not cut at the wrong place. A module under no source root at all (an ffi
   * stub) is namespaced by the folder holding it, which is the package it belongs to.
   */
  SPP_ATTR_NODISCARD auto NamespaceOf(
    std::filesystem::path const &module_path) const
    -> Vec<Str>;
};
