module;
#include <spp/macros.hpp>

export module spp.asts.mixins.temp_type_ast;

import std;


namespace spp::asts::mixins {
    SPP_EXP_CLS struct TempTypeAst;
}

namespace spp::asts {
    SPP_EXP_CLS struct TypeAst;
}


SPP_EXP_CLS struct spp::asts::mixins::TempTypeAst {
    TempTypeAst() = default;

    virtual ~TempTypeAst();

    /**
     * Convert this temporary type ast into a different type ast. For example, converting the array type st, parsed from
     * @code [Type, 3_uz]@endcode to @code std::array::Arr[T=Type, n=3_uz]@endcode.
     * @return
     */
    virtual auto convert() -> std::unique_ptr<TypeAst> = 0;
};
