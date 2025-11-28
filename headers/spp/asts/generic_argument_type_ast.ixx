module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_type_ast;
import spp.asts.generic_argument_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentTypeAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The GenericArgumentTypeAst represents a generic argument that accepts a type (not a compile time value).
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentTypeAst : GenericArgumentAst {
    /**
     * The value of the generic type argument. This is passed into the generic like @code func[T]()@endcode or
     * @code std::Vec[std::String]@endcode.
     */
    std::shared_ptr<TypeAst> val;

    /**
     * Construct the GenericArgumentTypeAst with the arguments matching the members.
     * @param val The value of the generic type argument.
     * @param order_tag The order tag for this argument, used to enforce ordering rules.
     */
    explicit GenericArgumentTypeAst(
        decltype(val) val,
        decltype(m_order_tag) order_tag);

    ~GenericArgumentTypeAst() override;

    auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void override;

protected:
    SPP_ATTR_NODISCARD auto equals_generic_argument_comp_keyword(GenericArgumentCompKeywordAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals_generic_argument_comp_positional(GenericArgumentCompPositionalAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals_generic_argument_type_keyword(GenericArgumentTypeKeywordAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals_generic_argument_type_positional(GenericArgumentTypePositionalAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals(GenericArgumentAst const &other) const -> std::strong_ordering override = 0;
};
