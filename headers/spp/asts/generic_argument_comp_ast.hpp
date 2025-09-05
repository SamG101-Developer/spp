#pragma once
#include <spp/asts/generic_argument_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The GenericArgumentCompAst represents a generic argument that accepts a compile time value (not a type). Any type is
 * allowed, as any type can be represented at compile time.
 */
struct spp::asts::GenericArgumentCompAst : GenericArgumentAst {
    /**
     * The value of the generic comp argument. This is passed into the generic like @code func[123]()@endcode or
     * @code std::Arr[std::String, 100_uz]@endcode.
     */
    std::unique_ptr<ExpressionAst> val;

    /**
     * Construct the GenericArgumentCompAst with the arguments matching the members.
     * @param val The value of the generic comp argument.
     */
    explicit GenericArgumentCompAst(
        decltype(val) &&val);

    ~GenericArgumentCompAst() override;
};
