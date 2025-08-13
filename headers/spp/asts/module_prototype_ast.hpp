#ifndef MODULE_PROTOTYPE_AST_HPP
#define MODULE_PROTOTYPE_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The ModulePrototypeAst represents a prototype for a module in the SPP language. It contains a the implementation of
 * the module.
 */
struct spp::asts::ModulePrototypeAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The module implementation AST that this prototype represents.
     */
    std::unique_ptr<ModuleImplementationAst> impl;

    /**
     * Construct the ModulePrototypeAst with the given implementation.
     * @param[in] impl The module implementation AST that this prototype represents.
     */
    explicit ModulePrototypeAst(
        decltype(impl) &&impl);
};


#endif //MODULE_PROTOTYPE_AST_HPP
