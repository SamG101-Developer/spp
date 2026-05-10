module;
#include <spp/macros.hpp>

export module spp.utils.algorithms;
import std;

namespace spp::utils::algorithms {
    SPP_EXP_FUN template <typename InputIt, typename T, typename BinOp>
    auto MoveAccumulate(InputIt first, InputIt last, T &&init, BinOp &&op) -> decltype(init);
}

SPP_EXP_FUN template <typename InputIt, typename T, typename BinOp>
auto spp::utils::algorithms::MoveAccumulate(InputIt first, InputIt last, T &&init, BinOp &&op) -> decltype(init) {
    for (; first != last; ++first) {
        init = std::forward<BinOp>(op)(std::forward<T>(init), std::move(*first));
    }
    return init;
}
