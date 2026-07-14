module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_type_positional_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_type_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentTypePositionalAst;
}

COMMON_AST_IMPORTS

/**
 * The GenericArgumentTypePositionalAst represents a positional argument in a generic argument context. It is forces the
 * argument to be matched by an index rather than a keyword.
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentTypePositionalAst final : GenericArgumentTypeAst {
    SPP_GCC_VTABLE_FIX

    /**
     * Construct the GenericArgumentTypePositionalAst with the arguments matching the members.
     * @param val The value of the generic type argument.
     */
    explicit GenericArgumentTypePositionalAst(
        decltype(Val) val);

    ~GenericArgumentTypePositionalAst() override;

    SPP_ATTR_NODISCARD auto EqualsGenericArgumentTypePositional(GenericArgumentTypePositionalAst const &other) const -> Ordering override;

    SPP_ATTR_NODISCARD auto Equals(GenericArgumentAst const &other) const -> Ordering override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::GenericArgumentTypePositionalAst)
