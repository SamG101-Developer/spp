module;
#include <spp/macros.hpp>

export module spp.utils.functions;
import std;

namespace spp::utils::functions {
    SPP_EXP_CLS template <typename... Ts>
    struct Overload : Ts... {
        using Ts::operator()...;
    };

    SPP_EXP_CLS template <typename... Ts>
    Overload(Ts...) -> Overload<Ts...>;
}
