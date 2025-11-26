module;
#include <spp/macros.hpp>

export module spp.utils.variants;


namespace spp::utils::variants {
    SPP_EXP template <typename... Ts>
    struct overload : Ts... {
        using Ts::operator()...;
    };

    SPP_EXP template <typename... Ts>
    overload(Ts...) -> overload<Ts...>;
}
