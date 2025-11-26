module;
#include <spp/macros.hpp>

export module spp.compiler.compiler;
import spp.analyse.scopes.scope_manager;
import spp.compiler.module_tree;
import spp.compiler.compiler_boot;

import std;


namespace spp::compiler {
    SPP_EXP class Compiler;
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
};


SPP_EXP class spp::compiler::Compiler {
public:
    enum class Mode {
        DEV, REL
    };

private:
    std::filesystem::path m_path;

    ModuleTree m_modules;

    Mode m_mode = Mode::DEV;

    CompilerBoot m_boot;

    std::unique_ptr<analyse::scopes::ScopeManager> m_scope_manager;

public:
    explicit Compiler(Mode mode);

    auto compile() -> void;

    static auto cleanup() -> void;
};
