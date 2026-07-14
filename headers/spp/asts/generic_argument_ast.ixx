module;
#include <spp/macros.hpp>

export module spp.asts.generic_argument_ast;
import spp.asts.ast;
import spp.asts.mixins.orderable_ast;
import spp.asts.utils.orderable;
import spp.utils.types;
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
    using make_keyword_arg_t = make_keyword_arg<T>::type;

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
    using make_positional_arg_t = make_positional_arg<T>::type;
}

/**
 * The GenericArgumentAst is the base class for all generic arguments. It is inherited by the @c GenericArgumentCompAst
 * and @c GenericArgumentTypeAst, which represent the two types of generic arguments in the language. These in turn are
 * inherited for the positional and keyword variants.
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentAst : Ast, mixins::OrderableAst {
    explicit GenericArgumentAst(utils::OrderableTag order_tag);
    ~GenericArgumentAst() override;
    auto operator<=>(GenericArgumentAst const &other) const -> Ordering;
    auto operator==(GenericArgumentAst const &other) const -> bool;

    SPP_ATTR_NODISCARD virtual auto EqualsGenericArgumentCompKeyword(GenericArgumentCompKeywordAst const &) const -> Ordering;
    SPP_ATTR_NODISCARD virtual auto EqualsGenericArgumentCompPositional(GenericArgumentCompPositionalAst const &) const -> Ordering;
    SPP_ATTR_NODISCARD virtual auto EqualsGenericArgumentTypeKeyword(GenericArgumentTypeKeywordAst const &) const -> Ordering;
    SPP_ATTR_NODISCARD virtual auto EqualsGenericArgumentTypePositional(GenericArgumentTypePositionalAst const &) const -> Ordering;
    SPP_ATTR_NODISCARD virtual auto Equals(GenericArgumentAst const &other) const -> Ordering = 0;
    SPP_ATTR_NODISCARD virtual auto ViewName() const -> StrView;
};
