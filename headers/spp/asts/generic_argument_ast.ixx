module;
#include <spp/macros.hpp>

export module spp.asts:generic_argument_ast;
import :ast;
import :orderable_ast;
import spp.asts.utils;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentCompKeywordAst;
    SPP_EXP_CLS struct GenericArgumentCompPositionalAst;
    SPP_EXP_CLS struct GenericArgumentTypeKeywordAst;
    SPP_EXP_CLS struct GenericArgumentTypePositionalAst;
}


/**
 * The GenericArgumentAst is the base class for all generic arguments. It is inherited by the @c GenericArgumentCompAst
 * and @c GenericArgumentTypeAst, which represent the two types of generic arguments in the language. These in turn are
 * inherited for the positional and keyword variants.
 */
SPP_EXP_CLS struct spp::asts::GenericArgumentAst : virtual Ast, mixins::OrderableAst {
    SPP_ATTR_NODISCARD virtual auto equals_generic_argument_comp_keyword(GenericArgumentCompKeywordAst const &other) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals_generic_argument_comp_positional(GenericArgumentCompPositionalAst const &other) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals_generic_argument_type_keyword(GenericArgumentTypeKeywordAst const &other) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals_generic_argument_type_positional(GenericArgumentTypePositionalAst const &other) const -> std::strong_ordering;
    SPP_ATTR_NODISCARD virtual auto equals(GenericArgumentAst const &other) const -> std::strong_ordering = 0;

    explicit GenericArgumentAst(utils::OrderableTag order_tag);
    ~GenericArgumentAst() override;
    auto operator<=>(GenericArgumentAst const &other) const -> std::strong_ordering;
    auto operator==(GenericArgumentAst const &other) const -> bool;
};
