module;
#include <spp/macros.hpp>

export module spp.utils.variants;


namespace spp::utils::variants {
    SPP_EXP_CLS template <typename... Ts>
    struct overload : Ts... {
        using Ts::operator()...;
    };

    SPP_EXP_CLS template <typename... Ts>
    overload(Ts...) -> overload<Ts...>;
}
