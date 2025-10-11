#pragma once
#include <spp/asts/ast.hpp>
#include <spp/asts/mixins/orderable_ast.hpp>


/// @cond
namespace spp::asts::detail {
    template <typename GenericArgType>
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

    template <typename T>
    using make_keyword_arg_t = typename make_keyword_arg<T>::type;

    template <typename GenericArgType>
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

    template <typename T>
    using make_positional_arg_t = typename make_positional_arg<T>::type;
}
/// @endcond


/**
 * The GenericArgumentAst is the base class for all generic arguments. It is inherited by the @c GenericArgumentCompAst
 * and @c GenericArgumentTypeAst, which represent the two types of generic arguments in the language. These in turn are
 * inherited for the positional and keyword variants.
 */
struct spp::asts::GenericArgumentAst : virtual Ast, mixins::OrderableAst {
    using Ast::Ast;
    friend struct GenericArgumentCompKeywordAst;
    friend struct GenericArgumentCompPositionalAst;
    friend struct GenericArgumentTypeKeywordAst;
    friend struct GenericArgumentTypePositionalAst;

protected:
    virtual auto equals_generic_argument_comp_keyword(GenericArgumentCompKeywordAst const &) const -> std::strong_ordering;
    virtual auto equals_generic_argument_comp_positional(GenericArgumentCompPositionalAst const &) const -> std::strong_ordering;
    virtual auto equals_generic_argument_type_keyword(GenericArgumentTypeKeywordAst const &) const -> std::strong_ordering;
    virtual auto equals_generic_argument_type_positional(GenericArgumentTypePositionalAst const &) const -> std::strong_ordering;
    virtual auto equals(GenericArgumentAst const &other) const -> std::strong_ordering = 0;

public:
    GenericArgumentAst(decltype(m_order_tag) order_tag);
    ~GenericArgumentAst() override;
    auto operator<=>(GenericArgumentAst const &other) const -> std::strong_ordering;
    auto operator==(GenericArgumentAst const &other) const -> bool;
};
