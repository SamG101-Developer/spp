module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_comp_ast;
import spp.asts.expression_ast;
import spp.asts.generic_argument_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentCompAst;
    SPP_EXP_CLS struct GenericArgumentCompKeywordAst;
    SPP_EXP_CLS struct GenericArgumentCompPositionalAst;
}


/**
 * The GenericArgumentCompAst represents a generic argument that accepts a compile time value (not a type). Any type is
 * allowed, as any type can be represented at compile time.
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentCompAst : GenericArgumentAst {
    std::unique_ptr<ExpressionAst> val;

    explicit GenericArgumentCompAst(
        decltype(val) &&val);
    ~GenericArgumentCompAst() override;
};
