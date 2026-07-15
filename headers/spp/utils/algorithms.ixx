module;
#include <spp/macros.hpp>
#include <genex/macros.hpp>

export module spp.utils.algorithms;
import spp.utils.types;
import genex;
import std;

namespace spp::utils::algorithms {
    SPP_EXP_FUN template <typename InputIt, typename T, typename BinOp>
    auto MoveAccumulate(InputIt first, InputIt last, T &&init, BinOp &&op) -> decltype(init);
}

namespace spp::views {
    SPP_EXP_CLS template <typename To>
    struct cast_unique_fn;

    export template <typename To>
    inline constexpr cast_unique_fn<To> cast_unique{};

    SPP_EXP_CLS template <typename To>
    struct cast_shared_fn;

    export template <typename To>
    inline constexpr cast_shared_fn<To> cast_shared{};
}

SPP_EXP_FUN template <typename InputIt, typename T, typename BinOp>
auto spp::utils::algorithms::MoveAccumulate(InputIt first, InputIt last, T &&init, BinOp &&op) -> decltype(init) {
    for (; first != last; ++first) {
        init = std::forward<BinOp>(op)(std::forward<T>(init), std::move(*first));
    }
    return init;
}

SPP_EXP_CLS template <typename To>
struct spp::views::cast_unique_fn {
    template <typename I, typename S>
    requires std::input_iterator<I> and std::sentinel_for<S, I>
    GENEX_INLINE auto operator()(I first, S last) const -> Vec<Unique<To>> {
        // return genex::views::filter(genex::views::transform(std::move(first), std::move(last), [](auto &&v) -> Unique<To> {
        //     auto *p = dynamic_cast<To*>(v.get());
        //     if (p != nullptr) { (void)v.release(); }
        //     return Unique<To>(p);
        // }), [](Unique<To> const &v) { return v != nullptr; });

        auto out = Vec<Unique<To>>(last);
        for (; first != last; ++first) {
            if (auto *p = dynamic_cast<To*>(first->get()); p != nullptr) {
                (void)first->release();
                out.EmplaceBack(Unique<To>(p));
            }
        }
        return out;
    }

    template <typename Rng>
    requires genex::input_range<Rng>
    GENEX_INLINE constexpr auto operator()(Rng &&rng) const {
        // return genex::views::filter(genex::views::transform(std::forward<Rng>(rng), [](auto &&v) -> Unique<To> {
        //     auto *p = dynamic_cast<To*>(v.get());
        //     if (p != nullptr) { (void)v.release(); }
        //     return Unique<To>(p);
        // }), [](Unique<To> const &v) { return v != nullptr; });

        auto out = Vec<Unique<To>>();
        for (auto &&v : rng) {
            if (auto *p = dynamic_cast<To*>(v.get()); p != nullptr) {
                (void)v.release();
                out.EmplaceBack(Unique<To>(p));
            }
        }
        return out;
    }

    GENEX_INLINE constexpr auto operator()() const noexcept(
        SAFE_CTOR(cast_unique_fn)) {
        return genex::meta::bind_back(cast_unique_fn{});
    }
};

SPP_EXP_CLS template <typename To>
struct spp::views::cast_shared_fn {
    template <typename I, typename S>
    requires std::input_iterator<I> and std::sentinel_for<S, I>
    GENEX_INLINE constexpr auto operator()(I first, S last) const {
        return genex::views::filter(genex::views::transform(std::move(first), std::move(last), [](auto &&v) -> Shared<To> {
            return std::dynamic_pointer_cast<To>(v);
        }), [](Shared<To> const &v) { return v != nullptr; });
    }

    template <typename Rng>
    requires genex::input_range<Rng>
    GENEX_INLINE constexpr auto operator()(Rng &&rng) const {
        return genex::views::filter(genex::views::transform(std::forward<Rng>(rng), [](auto &&v) -> Shared<To> {
            return std::dynamic_pointer_cast<To>(v);
        }), [](Shared<To> const &v) { return v != nullptr; });
    }

    GENEX_INLINE constexpr auto operator()() const noexcept(
        SAFE_CTOR(cast_shared_fn)) {
        return genex::meta::bind_back(cast_shared_fn{});
    }
};
