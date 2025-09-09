#pragma once
#include <array>
#include <filesystem>

#include <spp/compiler/compiler_boot.hpp>
#include <spp/compiler/module_tree.hpp>


namespace spp::compiler {
    struct Compiler;
}


inline constexpr auto COMPILER_STAGE_NAMES = std::array{
    "Lexing",
    "Parsing",
    "Pre-processing",
    "Generating Top-Level Scopes",
    "Generating Top-Level Aliases",
    "Qualifying Types",
    "Loading Super Scopes",
    "Pre-Analysing Semantics",
    "Analysing Semantics",
    "Checking Memory Safety"
};


struct spp::compiler::Compiler {
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
};
