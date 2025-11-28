module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_comp_ast;
import spp.asts._fwd;
import spp.asts.generic_argument_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentCompAst;
}


/**
 * The GenericArgumentCompAst represents a generic argument that accepts a compile time value (not a type). Any type is
 * allowed, as any type can be represented at compile time.
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentCompAst : GenericArgumentAst {
    /**
     * The value of the generic comp argument. This is passed into the generic like @code func[123]()@endcode or
     * @code std::Arr[std::String, 100_uz]@endcode.
     */
    std::unique_ptr<ExpressionAst> val;

    /**
     * Construct the GenericArgumentCompAst with the arguments matching the members.
     * @param val The value of the generic comp argument.
     * @param order_tag The order tag for this argument, used to enforce ordering rules.
     */
    explicit GenericArgumentCompAst(
        decltype(val) &&val,
        decltype(m_order_tag) order_tag);

    ~GenericArgumentCompAst() override;
};
