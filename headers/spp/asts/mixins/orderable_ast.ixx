module;
#include <spp/macros.hpp>

export module spp.asts.mixins.orderable_ast;
import spp.asts.utils.orderable;


namespace spp::asts::mixins {
    SPP_EXP_CLS struct OrderableAst;
}


/**
 * An @c OrderableAst is an AST that contains a vector attribute that required some sort of ordering based on type. For
 * example, all parameters and arguments are orderable, as they can be ordered in their groups. Required parameters must
 * precede optional parameters fr example.
 */
SPP_EXP_CLS struct spp::asts::mixins::OrderableAst {
private:
    utils::OrderableTag m_order_tag;

public:
    explicit OrderableAst(utils::OrderableTag order_tag);

    virtual ~OrderableAst();

    /**
     * Get the order tag of this orderable AST. This is used as a simple wrapping accessor for the order tag.
     * @return The order tag of this AST.
     */
    SPP_ATTR_NODISCARD auto get_order_tag() const -> utils::OrderableTag;
};
