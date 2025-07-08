#ifndef TEMP_TYPE_AST_HPP
#define TEMP_TYPE_AST_HPP

#include <spp/asts/_fwd.hpp>

namespace spp::asts::mixins {
    struct TempTypeAst;
}


struct spp::asts::mixins::TempTypeAst {
public:
    virtual ~TempTypeAst() = default;

public:
    /**
     * Convert this temporary type ast into a different type ast. For example, converting the array type st, parsed from
     * @code [Type, 3_uz]@endcode to @code std::array::Arr[T=Type, n=3_uz]@endcode.
     * @return
     */
    virtual auto convert() -> std::unique_ptr<TypeAst> = 0;
};


#endif //TEMP_TYPE_AST_HPP
