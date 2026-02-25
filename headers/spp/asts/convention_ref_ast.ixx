module;
#include <spp/macros.hpp>

export module spp.asts.convention_ref_ast;
import spp.asts.convention_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionRefAst;
}


/**
 * The ConventionRefAst represents a convention for immutable borrows. Immutable borrows can be taken from immutable or
 * mutable values.
 */
SPP_EXP_CLS struct spp::asts::ConventionRefAst final : ConventionAst {
    explicit ConventionRefAst();
    ~ConventionRefAst() override;
    auto to_rust() const -> std::string override;
};
