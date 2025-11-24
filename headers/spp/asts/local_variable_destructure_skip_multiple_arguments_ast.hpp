#pragma once
#include <spp/asts/local_variable_ast.hpp>


struct spp::asts::LocalVariableDestructureSkipMultipleArgumentsAst final : LocalVariableAst {
    friend struct CasePatternVariantDestructureSkipMultipleArgumentsAst;

    /**
     * The @c .. token indicates the skip multiple arguments pattern. This is used to indicate that a group of arguments
     * is being skipped. Bindings are used for array and tuple destructuring, while object destructuring can only use an
     * unbound multi skip.
     */
    std::unique_ptr<TokenAst> tok_ellipsis;

    /**
     * The optional binding for the skip multiple arguments pattern. This is used to bind the skipped arguments to a
     * variable, as an inner array or tuple (based on the outer type being destructured). No biding means that these
     * values are dropped.
     */
    std::unique_ptr<LocalVariableSingleIdentifierAst> binding;

    /**
     * Construct the LocalVariableDestructureSkipMultipleArgumentsAst with the arguments matching the members.
     * @param tok_ellipsis The @c .. token that indicates the skip multiple arguments pattern.
     * @param binding The optional binding for the skip multiple arguments pattern.
     */
    LocalVariableDestructureSkipMultipleArgumentsAst(
        decltype(tok_ellipsis) &&tok_ellipsis,
        std::unique_ptr<LocalVariableAst> &&binding); // cast in ctor

    ~LocalVariableDestructureSkipMultipleArgumentsAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto extract_name() const -> std::shared_ptr<IdentifierAst> override;

    auto extract_names() const -> std::vector<std::shared_ptr<IdentifierAst>> override;
};
