module;
#include <spp/macros.hpp>

export module spp.compiler.compiler;
import spp.utils.types;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS class ScopeManager;
}

namespace spp::compiler {
  SPP_EXP_CLS class Compiler;
  SPP_EXP_CLS struct CompilerBoot;
  SPP_EXP_CLS struct ModuleTree;
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

  bool m_for_unit_tests = false;

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

  explicit Compiler(Mode mode, BuildType build_type);

  static auto ForCppGoogleTest(Mode mode, Str &&main_code) -> Unique<Compiler>;

  ~Compiler();

  auto Compile() -> void;

  /**
   * The values the main module's compile-time constants resolved to, by name. This is the only way to observe what
   * comp-time resolution actually computed: nothing downstream of it reports a value, and the scopes holding them are
   * gone once @c Compile returns.
   * @return A map of constant name to the source text of the literal it resolved to.
   */
  SPP_ATTR_NODISCARD auto CompTimeConstants() const -> Map<Str, Str> const&;

  static auto Cleanup() -> void;
};
