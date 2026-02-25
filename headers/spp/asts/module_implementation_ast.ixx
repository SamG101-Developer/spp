module;
#include <spp/macros.hpp>

export module spp.asts.module_implementation_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ModuleImplementationAst;
    SPP_EXP_CLS struct ModuleMemberAst;
}


/**
 * The ModuleImplementationAst represents the implementation of a module in the SPP language. It contains a list of
 * module members that define the functionality and structure of the module.
 */
SPP_EXP_CLS struct spp::asts::ModuleImplementationAst final : Ast {
    std::vector<std::unique_ptr<ModuleMemberAst>> members;

    explicit ModuleImplementationAst(
        decltype(members) &&members);
    ~ModuleImplementationAst() override;
    auto to_rust() const -> std::string override;
};
