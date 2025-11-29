module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_ast;
import spp.asts.ast;
import spp.asts.mixins.orderable_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentCompAst;
    SPP_EXP_CLS struct GenericArgumentTypeAst;
    SPP_EXP_CLS struct GenericArgumentCompKeywordAst;
    SPP_EXP_CLS struct GenericArgumentCompPositionalAst;
    SPP_EXP_CLS struct GenericArgumentTypeKeywordAst;
    SPP_EXP_CLS struct GenericArgumentTypePositionalAst;
}


namespace spp::asts::detail {
    SPP_EXP_CLS template <typename GenericArgType>
    struct make_keyword_arg {
        using type = GenericArgType;
    };

    template <>
    struct make_keyword_arg<GenericArgumentCompAst> {
        using type = GenericArgumentCompKeywordAst;
    };

    template <>
    struct make_keyword_arg<GenericArgumentTypeAst> {
        using type = GenericArgumentTypeKeywordAst;
    };

    SPP_EXP_CLS template <typename T>
    using make_keyword_arg_t = typename make_keyword_arg<T>::type;


    SPP_EXP_CLS template <typename GenericArgType>
    struct make_positional_arg {
        using type = GenericArgType;
    };

    template <>
    struct make_positional_arg<GenericArgumentCompAst> {
        using type = GenericArgumentCompPositionalAst;
    };

    template <>
    struct make_positional_arg<GenericArgumentTypeAst> {
        using type = GenericArgumentTypePositionalAst;
    };

    SPP_EXP_CLS template <typename T>
    using make_positional_arg_t = typename make_positional_arg<T>::type;
}


/**
 * The GenericArgumentAst is the base class for all generic arguments. It is inherited by the @c GenericArgumentCompAst
 * and @c GenericArgumentTypeAst, which represent the two types of generic arguments in the language. These in turn are
 * inherited for the positional and keyword variants.
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentAst : virtual Ast, mixins::OrderableAst {
    using Ast::Ast;
    friend struct spp::asts::GenericArgumentCompKeywordAst;
    friend struct spp::asts::GenericArgumentCompPositionalAst;
    friend struct spp::asts::GenericArgumentTypeKeywordAst;
    friend struct spp::asts::GenericArgumentTypePositionalAst;

protected:
    SPP_ATTR_NODISCARD virtual auto equals_generic_argument_comp_keyword(GenericArgumentCompKeywordAst const &) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals_generic_argument_comp_positional(GenericArgumentCompPositionalAst const &) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals_generic_argument_type_keyword(GenericArgumentTypeKeywordAst const &) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals_generic_argument_type_positional(GenericArgumentTypePositionalAst const &) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals(GenericArgumentAst const &other) const -> std::strong_ordering;

public:
    explicit GenericArgumentAst(decltype(m_order_tag) order_tag);
    ~GenericArgumentAst() override;
    auto operator<=>(GenericArgumentAst const &other) const -> std::strong_ordering;
    auto operator==(GenericArgumentAst const &other) const -> bool;
};
