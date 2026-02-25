module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_comp_positional_ast;
import spp.asts.generic_argument_comp_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentCompPositionalAst;
}


/**
 * The GenericArgumentCompPositionalAst represents a positional argument in a generic argument context. It is forces the
 * argument to be matched by an index rather than a keyword.
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentCompPositionalAst final : GenericArgumentCompAst {
    explicit GenericArgumentCompPositionalAst(
        decltype(val) &&val);
    ~GenericArgumentCompPositionalAst() override;
    auto to_rust() const -> std::string override;
};
