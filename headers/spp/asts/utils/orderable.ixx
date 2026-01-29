module;
#include <spp/macros.hpp>

export module spp.asts.utils.orderable;


namespace spp::asts::utils {
    SPP_EXP_ENUM enum class OrderableTag;
}

SPP_EXP_ENUM enum class spp::asts::utils::OrderableTag {
    KEYWORD_ARG,
    POSITIONAL_ARG,
    SELF_PARAM,
    REQUIRED_PARAM,
    OPTIONAL_PARAM,
    VARIADIC_PARAM,
};
