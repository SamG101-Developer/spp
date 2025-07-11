#ifndef MODULE_IMPLEMENTATION_AST_HPP
#define MODULE_IMPLEMENTATION_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The ModuleImplementationAst represents the implementation of a module in the SPP language. It contains a list of
 * module members that define the functionality and structure of the module.
 */
struct spp::asts::ModuleImplementationAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The list of module members in the implementation. This can include function implementations, class implementations,
     * and other module-level constructs.
     */
    std::vector<std::unique_ptr<ModuleMemberAst>> members;

    /**
     * Construct the ModuleImplementationAst with the arguments matching the members.
     * @param[in] members The list of module members in the implementation.
     */
    explicit ModuleImplementationAst(
        decltype(members) &&members);
};


#endif //MODULE_IMPLEMENTATION_AST_HPP
