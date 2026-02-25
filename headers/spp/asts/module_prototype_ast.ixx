module;
#include <spp/macros.hpp>

export module spp.asts.module_prototype_ast;
import spp.asts.ast;
import std;

namespace spp::compiler {
    SPP_EXP_CLS struct CompilerBoot;
}

namespace spp::asts {
    SPP_EXP_CLS struct ModulePrototypeAst;
    SPP_EXP_CLS struct ModuleImplementationAst;
}


/**
 * The ModulePrototypeAst represents a prototype for a module in the SPP language. It contains a the implementation of
 * the module.
 */
SPP_EXP_CLS struct spp::asts::ModulePrototypeAst final : Ast {
    std::filesystem::path file_path = "";
    std::unique_ptr<ModuleImplementationAst> impl;

    explicit ModulePrototypeAst(
        decltype(impl) &&impl);
    ~ModulePrototypeAst() override;
    auto to_rust() const -> std::string override;
};
