#ifndef MODULE_PROTOTYPE_AST_HPP
#define MODULE_PROTOTYPE_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The ModulePrototypeAst represents a prototype for a module in the SPP language. It contains a the implementation of
 * the module.
 */
struct spp::asts::ModulePrototypeAst final : Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The module implementation AST that this prototype represents.
     */
    std::unique_ptr<ModuleImplementationAst> implementation;

    /**
     * Construct the ModulePrototypeAst with the given implementation.
     * @param[in] implementation The module implementation AST that this prototype represents.
     */
    explicit ModulePrototypeAst(
        std::unique_ptr<ModuleImplementationAst> &&implementation);
};


#endif //MODULE_PROTOTYPE_AST_HPP
