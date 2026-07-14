module;
#include <spp/macros.hpp>

export module spp.compiler.module_tree;
import spp.lex.tokens;
import spp.utils.error_formatter;
import spp.utils.types;
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
    Str code;
    Vec<lex::RawToken> tokens = {};
    Unique<asts::ModulePrototypeAst> module_ast;
    Unique<utils::errors::ErrorFormatter> error_formatter;

    Module(
        std::filesystem::path path,
        Str code,
        Vec<lex::RawToken> tokens,
        Unique<asts::ModulePrototypeAst> module_ast,
        Unique<utils::errors::ErrorFormatter> &&error_formatter);

    static auto FromPath(std::filesystem::path const &path);
};

SPP_EXP_CLS struct spp::compiler::ModuleTree {
private:
    std::filesystem::path m_root;
    std::filesystem::path m_src_path;
    std::filesystem::path m_vcs_path;
    std::filesystem::path m_ffi_path;
    Vec<Unique<Module>> m_modules;
    int m_lock_fd = -1;

    auto Lock() -> void;

    auto Unlock() const -> void;

public:
    explicit ModuleTree(std::filesystem::path path);

    static auto ForUnitTests(std::filesystem::path path, Str &&main_code) -> Unique<ModuleTree>;

    auto begin() -> Vec<Unique<Module>>::iterator;

    auto end() -> Vec<Unique<Module>>::iterator;

    auto GetModules() -> Vec<Module*>;

    auto RootPath() const -> std::filesystem::path;
};
