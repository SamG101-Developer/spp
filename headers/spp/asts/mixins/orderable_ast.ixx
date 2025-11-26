module;
#include <spp/macros.hpp>

export module spp.asts.mixins.orderable_ast;


/// @cond
namespace spp::asts::mixins {
    SPP_EXP struct OrderableAst;
    SPP_EXP enum class OrderableTag;
}
/// @endcond


/**
 * An @c OrderableAst is an AST that contains a vector attribute that required some sort of ordering based on type. For
 * example, all parameters and arguments are orderable, as they can be ordered in their groups. Required parameters must
 * precede optional parameters fr example.
 */
SPP_EXP struct spp::asts::mixins::OrderableAst {
protected:
    OrderableTag m_order_tag;

public:
    explicit OrderableAst(OrderableTag order_tag);

    virtual ~OrderableAst();

    auto get_order_tag() const -> OrderableTag;
};


SPP_EXP enum class spp::asts::mixins::OrderableTag {
    KEYWORD_ARG,
    POSITIONAL_ARG,
    SELF_PARAM,
    REQUIRED_PARAM,
    OPTIONAL_PARAM,
    VARIADIC_PARAM,
};
