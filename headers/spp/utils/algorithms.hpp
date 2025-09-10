#pragma once
#include <utility>


namespace spp::utils::algorithms {
    template <typename InputIt, typename T, typename BinOp>
    auto move_accumulate(InputIt first, InputIt last, T &&init, BinOp &&op) -> decltype(init);
}


template <typename InputIt, typename T, typename BinOp>
auto spp::utils::algorithms::move_accumulate(InputIt first, InputIt last, T &&init, BinOp &&op) -> decltype(init) {
    for (; first != last; ++first) {
        init = std::forward<BinOp>(op)(std::move(init), std::move(*first));
    }
    return init;
}
