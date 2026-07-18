module;
#include <spp/macros.hpp>

export module spp.asts.utils.orderable;


namespace spp::asts::utils {
    SPP_EXP_CLS enum class OrderableTag;
}

SPP_EXP_CLS enum class spp::asts::utils::OrderableTag {
    kKeywordArg,
    kPositionalArg,
    kSelfParam,
    kRequiredParam,
    kOptionalParam,
    kVariadicParam,
};
