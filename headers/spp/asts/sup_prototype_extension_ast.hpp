#ifndef SUP_PROTOTYPE_EXTENSION_AST_HPP
#define SUP_PROTOTYPE_EXTENSION_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The SupPrototypeExtensionAst represents a superimposition of a type over a type. This is used to "inherit" a type.
 * For example, to extend the @c A type with @c B, the following code can be used:
 * @code
 * sup A ext B {
 *     # Override any abstract or virtual methods from B here.
 * }
 * @endcode
 */
struct spp::asts::SupPrototypeExtensionAst final : virtual Ast {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The @c sup keyword that represents the start of the superimposition. This is used to indicate that a type is
     * being extended with additional methods.
     */
    std::unique_ptr<TokenAst> tok_sup;

    /**
     * The generics available for this superimposition. This is used to superimpose over generic types (all generics
     * must be used by the type being extended).
     */
    std::unique_ptr<GenericParameterGroupAst> generic_param_group;

    /**
     * The name of the type that is being extended. This is the type that will gain the additional methods defined in
     * the body of this superimposition.
     */
    std::unique_ptr<TypeAst> name;

    /**
     * The @c ext keyword that represents the type that is being extended. This is used to indicate that an extension,
     * and a method block, is being defined.
     */
    std::unique_ptr<TokenAst> tok_ext;

    /**
     * The name of the super class that is this type is being extended from. The attributes and methods of this type
     * will now be available on the superimposed type.
     */
    std::unique_ptr<TypeAst> super_class;

    /**
     * The body of the superimposition. This is a list of methods that are being added to the type. Each method is
     * defined as a FunctionPrototypeAst, which includes the method's name, parameters, and return type.
     */
    std::unique_ptr<SupImplementationAst> body;

    /**
     * Construct the SupPrototypeFunctionsAst with the arguments matching the members.
     * @param tok_sup The @c sup keyword that represents the start of the superimposition.
     * @param generic_param_group The generics available for this superimposition.
     * @param name The name of the type that is being extended.
     * @param tok_ext The @c ext keyword that represents the type that is being extended.
     * @param super_class The name of the super class that is this type is being extended from.
     * @param body The body of the superimposition.
     */
    SupPrototypeExtensionAst(
        decltype(tok_sup) &&tok_sup,
        decltype(generic_param_group) &&generic_param_group,
        decltype(name) &&name,
        decltype(tok_ext) &&tok_ext,
        decltype(super_class) &&super_class,
        decltype(body) &&body);
};


#endif //SUP_PROTOTYPE_EXTENSION_AST_HPP
