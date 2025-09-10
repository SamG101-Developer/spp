#pragma once
#include <spp/asts/generic_argument_ast.hpp>


/**
 * The GenericArgumentTypeAst represents a generic argument that accepts a type (not a compile time value).
 */
struct spp::asts::GenericArgumentTypeAst : GenericArgumentAst {
    /**
     * The value of the generic type argument. This is passed into the generic like @code func[T]()@endcode or
     * @code std::Vec[std::String]@endcode.
     */
    std::shared_ptr<TypeAst> val;

    /**
     * Construct the GenericArgumentTypeAst with the arguments matching the members.
     * @param val The value of the generic type argument.
     */
    explicit GenericArgumentTypeAst(
        decltype(val) val);

    ~GenericArgumentTypeAst() override;
};
