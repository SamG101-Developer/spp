module;
#include <spp/macros.hpp>

export module spp.compiler.module_tree;
import spp.lex.tokens;
import spp.utils.error_formatter;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct ModulePrototypeAst;
}

namespace spp::compiler {
    SPP_EXP_CLS struct Module;
    SPP_EXP_CLS struct ModuleTree;
}


SPP_EXP_CLS struct spp::compiler::Module {
    std::filesystem::path path = "";
    std::string code;
    std::vector<lex::RawToken> tokens = {};
    std::unique_ptr<asts::ModulePrototypeAst> module_ast;
    std::shared_ptr<utils::errors::ErrorFormatter> error_formatter;

    Module(
        std::filesystem::path path,
        std::string code,
        std::vector<lex::RawToken> tokens,
        std::unique_ptr<asts::ModulePrototypeAst> module_ast,
        std::shared_ptr<utils::errors::ErrorFormatter> error_formatter);

    static auto from_path(std::filesystem::path const &path);
};


SPP_EXP_CLS struct spp::compiler::ModuleTree {
private:
    std::filesystem::path m_root;
    std::filesystem::path m_src_path;
    std::filesystem::path m_vcs_path;
    std::filesystem::path m_ffi_path;
    std::vector<std::unique_ptr<Module>> m_modules;

public:
    explicit ModuleTree(std::filesystem::path path);

    auto begin() -> std::vector<std::unique_ptr<Module>>::iterator;

    auto end() -> std::vector<std::unique_ptr<Module>>::iterator;

    auto get_modules() -> std::vector<Module*>;
};
