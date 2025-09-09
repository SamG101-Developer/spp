#pragma once
#include <spp/lex/tokens.hpp>
#include <spp/asts/_fwd.hpp>
#include <spp/pch.hpp>
#include <spp/utils/error_formatter.hpp>


namespace spp::compiler {
    struct Module;
    struct ModuleTree;
}


struct spp::compiler::Module {
    std::filesystem::path path = "";
    std::string code;
    std::vector<lex::RawToken> tokens = {};
    std::unique_ptr<asts::ModulePrototypeAst> module_ast;
    std::shared_ptr<utils::errors::ErrorFormatter> error_formatter;

    static auto from_path(std::filesystem::path const &path);
};


struct spp::compiler::ModuleTree {
private:
    std::filesystem::path m_root;
    std::filesystem::path m_src_path;
    std::filesystem::path m_vcs_path;
    std::filesystem::path m_ffi_path;
    std::vector<Module> m_modules;

public:
    explicit ModuleTree(std::filesystem::path path);

    auto begin() -> std::vector<Module>::iterator;

    auto end() -> std::vector<Module>::iterator;

    auto get_modules() -> std::vector<Module>&;
};
