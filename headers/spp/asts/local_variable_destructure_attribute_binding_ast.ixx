module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_attribute_binding_ast;
import spp.asts.local_variable_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct CasePatternVariantDestructureAttributeBindingAst;
    SPP_EXP_CLS struct LocalVariableDestructureAttributeBindingAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TokenAst;
}


SPP_EXP_CLS struct spp::asts::LocalVariableDestructureAttributeBindingAst final : LocalVariableAst {
    /**
     * The name of the attribute. This is the identifier that is used to refer to the attribute of the object being
     * destructured.
     */
    std::shared_ptr<IdentifierAst> name;

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

    SPP_AST_KEY_FUNCTIONS;

    auto extract_name() const -> std::shared_ptr<IdentifierAst> override;
};
