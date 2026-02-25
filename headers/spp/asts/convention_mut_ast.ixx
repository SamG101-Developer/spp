module;
#include <spp/macros.hpp>

export module spp.asts.convention_mut_ast;
import spp.asts.convention_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionMutAst;
}


/**
 * The ConventionMutAst represents a convention for mutable borrows. If the borrow is for an argument, this symbol must
 * be mutably defined.
 */
SPP_EXP_CLS struct spp::asts::ConventionMutAst final : ConventionAst {
    explicit ConventionMutAst();
    ~ConventionMutAst() override;
    auto to_rust() const -> std::string override;
};
