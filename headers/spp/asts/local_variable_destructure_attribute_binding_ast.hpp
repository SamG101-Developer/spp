#ifndef LOCAL_VARIABLE_DESTRUCTURE_ATTRIBUTE_BINDING_AST_HPP
#define LOCAL_VARIABLE_DESTRUCTURE_ATTRIBUTE_BINDING_AST_HPP

#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::LocalVariableDestructureAttributeBindingAst final : LocalVariableAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The name of the attribute. This is the identifier that is used to refer to the attribute of the object being
     * destructured.
     */
    std::unique_ptr<IdentifierAst> name;

    /**
     * The @c = token that separates the attribute name from the value in the destructure binding.
     */
    std::unique_ptr<TokenAst> tok_assign;

    /**
     * The value of the attribute. This can be a further destructure or a literal.
     */
    std::unique_ptr<LocalVariableAst> val;

    /**
     * Construct the LocalVariableDestructureAttributeBindingAst with the arguments matching the members.
     * @param name The name of the attribute.
     * @param tok_assign The @c = token that separates the attribute name from the value in the destructure binding.
     * @param val The value of the attribute.
     */
    LocalVariableDestructureAttributeBindingAst(
        decltype(name) &&name,
        decltype(tok_assign) &&tok_assign,
        decltype(val) &&val);

    ~LocalVariableDestructureAttributeBindingAst() override;
};


#endif //LOCAL_VARIABLE_DESTRUCTURE_ATTRIBUTE_BINDING_AST_HPP
