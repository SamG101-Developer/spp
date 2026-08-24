module;
#include <spp/macros.hpp>

export module spp.compiler.compiler;
import spp.compiler.module_tree;
import spp.utils.types;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS class ScopeManager;
}

namespace spp::compiler {
  SPP_EXP_CLS class Compiler;
  SPP_EXP_CLS struct CompilerBoot;
}

inline constexpr auto kCompilerStageNames = std::array{
  "Lexing......................... ",
  "Parsing........................ ",
  "Pre-processing................. ",
  "Generating Top-Level Scopes.... ",
  "Generating Top-Level Aliases... ",
  "Qualifying Types............... ",
  "Loading Super Scopes........... ",
  "Pre-Analysing Semantics........ ",
  "Analysing Semantics............ ",
  "Checking Memory Safety......... ",
  "Resolving comptime constants... ",
  "Monomorphising generics........ ",
  "Generating Code (1)............ ",
  "Generating Code (2)............ "
};

SPP_EXP_CLS class spp::compiler::Compiler {
public:
  enum class Mode { DEV, REL };

  enum class BuildType { EXE, LIB };

private:
  std::filesystem::path m_path;

  Unique<ModuleTree> m_modules;

  Mode m_mode = Mode::DEV;

  BuildType m_build_type = BuildType::EXE;

  Unique<CompilerBoot> m_boot;

  Unique<analyse::scopes::ScopeManager> m_scope_manager;

  bool m_for_cpp_google_test = false;

  /** How many unit tests the generated harness runs. Read off the boot once parsing has written the harness. */
  std::size_t m_test_count = 0;

  /** The names of those tests, kept past @c Cleanup so the driver can re-run them one at a time. */
  Vec<Str> m_test_names;

  /**
   * The compile-time constants the main module declared, and the values they resolved to, rendered as the source text
   * of the literal each one became. Filled once comp-time resolution has finished and kept past @c Cleanup , which
   * destroys the scopes the values were read off.
   */
  Map<Str, Str> m_comp_time_constants;

  /**
   * Read the resolved value of every @c cmp the main module declares off its scope, into
   * @c m_comp_time_constants . Compiler-generated names (the @c "$" mocks a @c cmp function produces) are left out,
   * because they name machinery rather than anything that was written.
   */
  auto CollectCompTimeConstants() -> void;

public:
  Compiler() = default; // TODO: Private

  /**
   * @param[in] mode Whether to build for development or release.
   * @param[in] build_type Whether the project produces an executable or a library.
   * @param[in] tests Which "tst" folders to compile alongside the sources. Empty for an ordinary build.
   */
  explicit Compiler(Mode mode, BuildType build_type, TestScope const &tests = {});

  static auto ForCppGoogleTest(Mode mode, Str &&main_code) -> Unique<Compiler>;

  ~Compiler();

  auto Compile() -> void;

  /**
   * Restrict which unit tests the generated harness runs. Must be set before @c Compile , because the filters are
   * applied while the harness is written rather than when it runs.
   */
  auto SetTestFilters(Str name_filter, Str group_filter) -> void;

  /** How many unit tests the generated harness ended up running. Valid once @c Compile has run. */
  SPP_ATTR_NODISCARD auto TestCount() const -> std::size_t;

  /** The fully qualified name of each test the harness runs. Valid once @c Compile has run. */
  SPP_ATTR_NODISCARD auto TestNames() const -> Vec<Str> const&;

  /**
   * The values the main module's compile-time constants resolved to, by name. This is the only way to observe what
   * comp-time resolution actually computed: nothing downstream of it reports a value, and the scopes holding them are
   * gone once @c Compile returns.
   * @return A map of constant name to the source text of the literal it resolved to.
   */
  SPP_ATTR_NODISCARD auto CompTimeConstants() const -> Map<Str, Str> const&;

  static auto Cleanup() -> void;
};
