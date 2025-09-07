#pragma once
#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


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


/**
 * The GenericArgumentAst is the base class for all generic arguments. It is inherited by the GenericArgumentCompAst and
 * GenericArgumentTypeAst, which represent the two types of generic arguments in the language. These in turn are
 * inherited for the positional and keyword variants.
 */
struct spp::asts::GenericArgumentAst : virtual Ast {
    using Ast::Ast;
    friend struct GenericArgumentCompKeywordAst;
    friend struct GenericArgumentCompPositionalAst;
    friend struct GenericArgumentTypeKeywordAst;
    friend struct GenericArgumentTypePositionalAst;

    friend auto operator==(GenericArgumentAst const &lhs_arg, GenericArgumentAst const &rhs_arg) -> bool {
        return lhs_arg.equals(rhs_arg);
    }

protected:
    virtual auto equals_generic_argument_comp_keyword(GenericArgumentCompKeywordAst const &) const -> bool;
    virtual auto equals_generic_argument_comp_positional(GenericArgumentCompPositionalAst const &) const -> bool;
    virtual auto equals_generic_argument_type_keyword(GenericArgumentTypeKeywordAst const &) const -> bool;
    virtual auto equals_generic_argument_type_positional(GenericArgumentTypePositionalAst const &) const -> bool;
    virtual auto equals(GenericArgumentAst const &other) const -> bool = 0;
};
