module;
#include <spp/macros.hpp>

export module spp.utils.algorithms;
import spp.utils.types;
import std;
import sys;

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

namespace spp::meta {
    struct deref_fn {
        template <typename T>
        constexpr auto operator()(T *value) const -> T& {
            return *value;
        }
    };

    export inline deref_fn deref{};
}

namespace spp::detail {
    template <std::ranges::input_range R, typename C, typename P>
    auto do_duplicates(R r, C comp, P proj) -> std::generator<std::ranges::range_value_t<R>> {
        using V = std::ranges::range_value_t<R>;
        std::vector<V> seen;
        std::optional<V> dupe;

        for (auto &&x : r) {
            decltype(auto) cur = std::invoke(proj, x);
            if (dupe) {
                if (comp(cur, std::invoke(proj, *dupe))) { co_yield static_cast<V>(x); }
                continue;
            }

            for (const auto &s : seen) {
                if (comp(cur, std::invoke(proj, s))) {
                    dupe = s;
                    co_yield static_cast<V>(*dupe);
                    co_yield static_cast<V>(x);
                    break;
                }
            }
            if (!dupe) seen.emplace_back(x);
        }
    }

    template <bool Want, typename Haystack, typename Pred1, typename Pred2>
    struct in_closure : std::ranges::range_adaptor_closure<in_closure<Want, Haystack, Pred1, Pred2>> {
        Haystack haystack;
        Pred1 pred1;
        Pred2 pred2;

        template <std::ranges::viewable_range Rng>
        constexpr auto operator()(Rng &&rng) const {
            return std::views::filter(
                std::forward<Rng>(rng),
                [hay = this->haystack, pe = this->pred1, ph = this->pred2](auto &&e) {
                    auto &&key = std::invoke(pe, e);
                    const auto found = std::ranges::any_of(hay, [&](auto &&h) { return std::invoke(ph, h) == key; });
                    return found == Want;
                });
        }
    };

    struct drop_last_closure : std::ranges::range_adaptor_closure<drop_last_closure> {
        std::size_t n;

        template <std::ranges::sized_range Rng> requires std::ranges::viewable_range<Rng>
        constexpr auto operator()(Rng &&rng) const {
            auto const sz = std::ranges::size(rng);
            auto const keep = sz > n ? sz - n : static_cast<std::ranges::range_size_t<Rng>>(0);
            return std::views::take(std::forward<Rng>(rng), static_cast<sys::ssize_t>(keep));
        }
    };

    template <class Pred>
    struct remove_if_closure : std::ranges::range_adaptor_closure<remove_if_closure<Pred>> {
        Pred pred;

        template <std::ranges::viewable_range Rng>
        constexpr auto operator()(Rng &&rng) const {
            return std::views::filter(std::forward<Rng>(rng), std::not_fn(pred));
        }
    };
}

namespace spp::views {
    struct ptr_fn : std::ranges::range_adaptor_closure<ptr_fn> {
        template <std::ranges::viewable_range Rng>
        constexpr auto operator()(Rng &&rng) const {
            return std::views::transform(std::forward<Rng>(rng), [](auto &&ptr) { return ptr.Get(); });
        }
    };

    template <typename To>
    struct cast_dynamic_fn : std::ranges::range_adaptor_closure<cast_dynamic_fn<To>> {
        // upcast -> no null check
        template <std::ranges::viewable_range Rng>
            requires std::derived_from<std::remove_pointer_t<std::ranges::range_value_t<Rng>>, std::remove_pointer_t<To>>
        constexpr auto operator()(Rng &&rng) const {
            return std::views::transform(std::forward<Rng>(rng), [](auto *ptr) { return static_cast<To>(ptr); });
        }

        // downcast -> needs null check
        template <std::ranges::viewable_range Rng>
            requires (
                not std::derived_from<std::remove_pointer_t<std::ranges::range_value_t<Rng>>, std::remove_pointer_t<To>> and
                std::derived_from<std::remove_pointer_t<To>, std::remove_pointer_t<std::ranges::range_value_t<Rng>>>)
        constexpr auto operator()(Rng &&rng) const {
            return std::views::transform(std::forward<Rng>(rng), [](auto *ptr) { return dynamic_cast<To>(ptr); }) | std::views::filter([](auto *ptr) { return ptr != nullptr; });
        }
    };

    // The class pointed to by the `Unique<X>` element type of a range.
    template <typename Rng>
    using unique_pointee_t = std::remove_reference_t<decltype(*std::declval<std::ranges::range_value_t<Rng>&>())>;

    template <typename To>
    struct cast_unique_fn : std::ranges::range_adaptor_closure<cast_unique_fn<To>> {
        // upcast (incl. same type) -> no null check
        template <std::ranges::viewable_range Rng> requires std::derived_from<unique_pointee_t<Rng>, To>
        constexpr auto operator()(Rng &&rng) const {
            return std::views::transform(std::forward<Rng>(rng), [](auto &&ptr) { return Unique<To>(static_cast<To*>(ptr.Release())); });
        }

        // downcast -> needs null check
        template <std::ranges::viewable_range Rng>
            requires (not std::derived_from<unique_pointee_t<Rng>, To> and std::derived_from<To, unique_pointee_t<Rng>>)
        constexpr auto operator()(Rng &&rng) const {
            return std::views::transform(std::forward<Rng>(rng), [](auto &&ptr) { return Unique<To>(dynamic_cast<To*>(ptr.Release())); }) | std::views::filter([](auto &&ptr) { return ptr == nullptr; });
        }
    };

    struct remove_if_fn : std::ranges::range_adaptor_closure<remove_if_fn> {
        template <typename Pred>
        constexpr auto operator()(Pred &&pred) const {
            return detail::remove_if_closure<std::decay_t<Pred>>{{}, std::forward<Pred>(pred)};
        }
    };

    struct drop_last_fn : std::ranges::range_adaptor_closure<drop_last_fn> {
        constexpr auto operator()(const std::size_t n) const {
            return detail::drop_last_closure{{}, n};
        }
    };

    struct move_fn : std::ranges::range_adaptor_closure<move_fn> {
        template <std::ranges::viewable_range Rng>
        constexpr auto operator()(Rng &&rng) const {
            return std::views::transform(std::forward<Rng>(rng), [](auto &&val) { return std::move(val); });
        }
    };

    template <bool Want>
    struct in_fn : std::ranges::range_adaptor_closure<in_fn<Want>> {
        template <std::ranges::viewable_range Hay, typename Pred1 = std::identity, typename Pred2 = std::identity>
        auto operator()(Hay &&hay, Pred1 pe = {}, Pred2 ph = {}) const {
            auto view = std::views::all(std::forward<Hay>(hay));
            return detail::in_closure<Want, decltype(view), Pred1, Pred2>{{}, std::move(view), std::move(pe), std::move(ph)};
        }
    };

    struct remove_fn : std::ranges::range_adaptor_closure<remove_fn> {
        template <typename T, typename Pred1 = std::identity, typename Pred2 = std::identity>
        constexpr auto operator()(T &&value, Pred1 p1 = {}, Pred2 p2 = {}) const {
            return std::views::filter([value = std::forward<T>(value), p1 = std::move(p1), p2 = std::move(p2)](auto &&x) {
                return std::invoke(p1, x) != std::invoke(p2, value);
            });
        }
    };

    template <std::size_t N>
    struct get_nth_fn : std::ranges::range_adaptor_closure<get_nth_fn<N>> {
        template <std::ranges::viewable_range Rng>
        constexpr auto operator()(Rng &&rng) const {
            return std::views::transform(std::forward<Rng>(rng), []<typename T>(T &&val) { return std::get<N>(std::forward<T>(val)); });
        }
    };

    struct eq_fn {
        template <typename T, typename U>
        constexpr auto operator()(T &&a, U &&b) const -> bool {
            return std::forward<T>(a) == std::forward<U>(b);
        }

        template <typename T, typename U, typename Pred1 = std::identity, typename Pred2 = std::identity>
        constexpr auto operator()(T &&a, U &&b, Pred1 p1 = {}, Pred2 p2 = {}) const -> bool {
            return p1(std::forward<T>(a)) == p2(std::forward<U>(b));
        }
    };

    template <class Comp = std::ranges::equal_to, class Proj = std::identity>
    struct duplicates_fn : std::ranges::range_adaptor_closure<duplicates_fn<Comp, Proj>> {
        Comp comp{};
        Proj proj{};

        template <std::ranges::viewable_range R>
        auto operator()(R &&r) const {
            return detail::do_duplicates(std::views::all(std::forward<R>(r)), comp, proj);
        }

        template <class C = std::ranges::equal_to, class P = std::identity> requires (!std::ranges::viewable_range<C>)
        auto operator()(C c, P p = {}) const {
            return duplicates_fn<C, P>{{}, std::move(c), std::move(p)};
        }
    };

    struct cycle_fn : std::ranges::range_adaptor_closure<cycle_fn> {
        template <std::ranges::viewable_range R>
        constexpr auto operator()(R &&r) const {
            return std::views::repeat(std::views::all(std::forward<R>(r))) | std::views::join;
        }
    };

    export inline constexpr ptr_fn ptr{};
    export template <typename To>
    inline constexpr cast_dynamic_fn<To> cast_dynamic{};
    export template <typename To>
    inline constexpr cast_unique_fn<To> cast_unique{};
    export inline constexpr remove_if_fn remove_if{};
    export inline constexpr drop_last_fn drop_last{};
    export inline constexpr move_fn move{};
    export inline constexpr in_fn<true> in{};
    export inline constexpr in_fn<false> not_in{};
    export inline constexpr remove_fn remove{};
    export template <std::size_t N>
    inline constexpr get_nth_fn<N> get_nth{};
    export inline constexpr eq_fn eq{};
    export inline constexpr duplicates_fn duplicates{};
    export inline constexpr cycle_fn cycle{};
}
