module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_ast;
import spp.asts.utils.orderable;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentTypeAst;
    SPP_EXP_CLS struct GenericArgumentTypeKeywordAst;
    SPP_EXP_CLS struct GenericArgumentTypePositionalAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The GenericArgumentTypeAst represents a generic argument that accepts a type (not a compile time value).
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentTypeAst : GenericArgumentAst {
    SPP_GCC_VTABLE_FIX

    /**
     * The value of the generic type argument. This is passed into the generic like @code func[T]()@endcode or
     * @code std::Vec[Str]@endcode.
     */
    Shared<TypeAst> Val;

    struct {
        std::size_t OriginalValPosStart;
        std::size_t OriginalValPosEnd;
    } Source;

    /**
     * Construct the GenericArgumentTypeAst with the arguments matching the members.
     * @param val The value of the generic type argument.
     * @param order_tag The order tag for this argument, used to enforce ordering rules.
     */
    explicit GenericArgumentTypeAst(
        decltype(Val) val,
        utils::OrderableTag order_tag);

    ~GenericArgumentTypeAst() override;

    auto Stage4_QualifyTypes(ScopeManager *sm, CompilerMetaData *meta) -> void override;
};


SPP_GCC_VTABLE_FIX_IMPL(spp::asts::GenericArgumentTypeAst)
