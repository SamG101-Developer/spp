module;
#include <spp/macros.hpp>

export module spp.compiler.compiler;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class ScopeManager;
}

namespace spp::compiler {
    SPP_EXP_CLS class Compiler;
    SPP_EXP_CLS struct CompilerBoot;
    SPP_EXP_CLS struct ModuleTree;
}


inline constexpr auto COMPILER_STAGE_NAMES = std::array{
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
    enum class Mode {
        DEV, REL
    };

private:
    std::filesystem::path m_path;

    std::unique_ptr<ModuleTree> m_modules;

    Mode m_mode = Mode::DEV;

    std::unique_ptr<CompilerBoot> m_boot;

    std::unique_ptr<analyse::scopes::ScopeManager> m_scope_manager;

    bool m_for_unit_tests = false;

public:
    Compiler() = default; // TODO: Private

    explicit Compiler(Mode mode);

    static auto for_unit_tests(Mode mode, std::string &&main_code) -> std::unique_ptr<Compiler>;

    ~Compiler();

    auto compile() -> void;

    static auto cleanup() -> void;
};
