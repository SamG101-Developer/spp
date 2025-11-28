module;
#include <spp/macros.hpp>

export module spp.asts.local_variable_destructure_skip_single_argument_ast;
import spp.asts._fwd;
import spp.asts.local_variable_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct LocalVariableDestructureSkipSingleArgumentAst;
}


SPP_EXP_CLS struct spp::asts::LocalVariableDestructureSkipSingleArgumentAst final : LocalVariableAst {
    friend struct spp::asts::CasePatternVariantDestructureSkipSingleArgumentAst;

    /**
     * The @c _ token that indicates the skip single argument pattern. This is used to indicate the next element
     * sequentially is being skipped, and is often seen in array and tuple destructuring. Invalid in object
     * destructuring as it is purely keyword based, and not positional.
     */
    std::unique_ptr<TokenAst> tok_underscore;

    /**
     * Construct the LocalVariableDestructureSkipSingleArgumentAst with the arguments matching the members.
     * @param tok_underscore The @c _ token that indicates the skip single argument pattern.
     */
    explicit LocalVariableDestructureSkipSingleArgumentAst(
        decltype(tok_underscore) &&tok_underscore);

    ~LocalVariableDestructureSkipSingleArgumentAst() override;

    SPP_AST_KEY_FUNCTIONS;

    auto extract_name() const -> std::shared_ptr<IdentifierAst> override;

    auto extract_names() const -> std::vector<std::shared_ptr<IdentifierAst>> override;
};
