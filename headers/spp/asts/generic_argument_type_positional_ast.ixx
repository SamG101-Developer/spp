module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_type_positional_ast;
import spp.asts._fwd;
import spp.asts.generic_argument_type_ast;

import std;


/**
 * The GenericArgumentTypePositionalAst represents a positional argument in a generic argument context. It is forces the
 * argument to be matched by an index rather than a keyword.
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentTypePositionalAst final : GenericArgumentTypeAst {
    /**
     * Construct the GenericArgumentTypePositionalAst with the arguments matching the members.
     * @param val The value of the generic type argument.
     */
    explicit GenericArgumentTypePositionalAst(
        decltype(val) val);

    ~GenericArgumentTypePositionalAst() override;

protected:
    auto equals(GenericArgumentAst const &other) const -> std::strong_ordering override;

    auto equals_generic_argument_type_positional(GenericArgumentTypePositionalAst const &) const -> std::strong_ordering override;

public:
    SPP_AST_KEY_FUNCTIONS;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};
