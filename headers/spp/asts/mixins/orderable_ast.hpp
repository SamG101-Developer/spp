#pragma once

#include <spp/asts/_fwd.hpp>


namespace spp::asts::mixins {
    struct OrderableAst;
    enum class OrderableTag;
}


/**
 * An @c OrderableAst is an AST that contains a vector attribute that required some sort of ordering based on type. For
 * example, all parameters and arguments are orderable, as they can be ordered in their groups. Required parameters must
 * precede optional parameters fr example.
 */
struct spp::asts::mixins::OrderableAst {
protected:
    OrderableTag m_order_tag;

public:
    explicit OrderableAst(OrderableTag order_tag);

    virtual ~OrderableAst();

    auto get_order_tag() const -> OrderableTag;
};


enum class spp::asts::mixins::OrderableTag {
    KEYWORD_ARG,
    POSITIONAL_ARG,
    SELF_PARAM,
    REQUIRED_PARAM,
    OPTIONAL_PARAM,
    VARIADIC_PARAM,
};
