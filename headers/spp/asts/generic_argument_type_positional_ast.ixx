module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_type_positional_ast;
import spp.asts.generic_argument_type_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentTypePositionalAst;
}


/**
 * The GenericArgumentTypePositionalAst represents a positional argument in a generic argument context. It is forces the
 * argument to be matched by an index rather than a keyword.
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentTypePositionalAst final : GenericArgumentTypeAst {
    explicit GenericArgumentTypePositionalAst(
        decltype(val) val);
    ~GenericArgumentTypePositionalAst() override;
    auto to_rust() const -> std::string override;
};
