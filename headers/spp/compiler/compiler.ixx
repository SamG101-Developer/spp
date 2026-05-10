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

public:
    Compiler() = default; // TODO: Private

    explicit Compiler(Mode mode, BuildType build_type);

    static auto ForUnitTests(Mode mode, Str &&main_code) -> Unique<Compiler>;

    ~Compiler();

    auto Compile() -> void;

    static auto Cleanup() -> void;
};
