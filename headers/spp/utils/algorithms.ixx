module;
#include <spp/macros.hpp>

export module spp.utils.algorithms;
import std;


namespace spp::utils::algorithms {
    SPP_EXP template <typename InputIt, typename T, typename BinOp>
    auto move_accumulate(InputIt first, InputIt last, T &&init, BinOp &&op) -> decltype(init);
}


SPP_EXP template <typename InputIt, typename T, typename BinOp>
auto spp::utils::algorithms::move_accumulate(InputIt first, InputIt last, T &&init, BinOp &&op) -> decltype(init) {
    for (; first != last; ++first) {
        init = std::forward<BinOp>(op)(std::forward<T>(init), std::move(*first));
    }
    return init;
}
