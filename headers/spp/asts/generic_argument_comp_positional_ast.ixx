module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_comp_positional_ast;
import spp.asts.generic_argument_ast;
import spp.asts.generic_argument_comp_ast;
import spp.asts.mixins.compiler_stages;
import spp.utils.types;
import std;

namespace spp::analyse::scopes {
    SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentCompPositionalAst;
}

COMMON_AST_IMPORTS

/**
 * The GenericArgumentCompPositionalAst represents a positional argument in a generic argument context. It is forces the
 * argument to be matched by an index rather than a keyword.
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentCompPositionalAst final : GenericArgumentCompAst {
    SPP_GCC_VTABLE_FIX

    /**
     * Construct the GenericArgumentCompPositionalAst with the arguments matching the members.
     * @param val The value of the generic comp argument.
     */
    explicit GenericArgumentCompPositionalAst(
        decltype(Val) &&val);

    ~GenericArgumentCompPositionalAst() override;

    SPP_ATTR_NODISCARD auto EqualsGenericArgumentCompPositional(GenericArgumentCompPositionalAst const &other) const -> Ordering override;

    SPP_ATTR_NODISCARD auto Equals(GenericArgumentAst const &other) const -> Ordering override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage8_CheckMemory(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::GenericArgumentCompPositionalAst)
