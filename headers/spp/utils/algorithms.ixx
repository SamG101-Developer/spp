module;
#include <genex/macros.hpp>
#include <spp/macros.hpp>

export module spp.utils.algorithms;
import spp.utils.ptr;
import spp.utils.types;
import genex;
import std;

namespace spp::utils::algorithms {
  SPP_EXP_FUN
  template <typename InputIt, typename T, typename BinOp>
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

  SPP_EXP_CLS template <std::size_t N>
  struct tuple_nth_fn;

  export template <std::size_t N>
  inline constexpr tuple_nth_fn<N> tuple_nth{};
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
    return genex::views::filter(
      genex::views::transform(std::move(first), std::move(last), [](auto &&v) -> Shared<To> {
        return spp::dynamic_shared_cast<To>(v);
      }), [](Shared<To> const &v) { return v != nullptr; });
  }

  template <typename Rng>
    requires genex::input_range<Rng>
  GENEX_INLINE constexpr auto operator()(Rng &&rng) const {
    return genex::views::filter(
      genex::views::transform(std::forward<Rng>(rng), [](auto &&v) -> Shared<To> {
        return spp::dynamic_shared_cast<To>(v);
      }), [](Shared<To> const &v) { return v != nullptr; });
  }

  GENEX_INLINE constexpr auto operator()() const noexcept(
    SAFE_CTOR(cast_shared_fn)) {
    return genex::meta::bind_back(cast_shared_fn{});
  }
};

SPP_EXP_CLS template <std::size_t N>
struct spp::views::tuple_nth_fn {
  template <typename I, typename S>
    requires std::input_iterator<I> and std::sentinel_for<S, I>
  GENEX_INLINE constexpr auto operator()(I first, S last) const noexcept(
    SAFE_CALL(decltype(genex::views::transform), I, S, genex::meta::identity) and
    SAFE_MOVE(I) and SAFE_MOVE(S)) {
    auto func = [](auto &&x) { return spp::get<N>(x); };
    return genex::views::transform(std::move(first), std::move(last), std::move(func));
  }

  template <typename Rng>
    requires genex::input_range<Rng>
  GENEX_INLINE constexpr auto operator()(Rng &&rng) const noexcept(
    SAFE_CALL(decltype(genex::views::transform), genex::iterator_t<Rng>, genex::sentinel_t<Rng>, genex::meta::identity) and
    SAFE_MOVE(Rng)) {
    auto [first, last] = genex::iterators::iter_pair(rng);
    auto func = [](auto &&x) { return spp::get<N>(x); };
    return genex::views::transform(std::move(first), std::move(last), std::move(func));
  }

  GENEX_INLINE constexpr auto operator()() const noexcept(
    SAFE_CTOR(tuple_nth_fn)) {
    return genex::meta::bind_back(tuple_nth_fn{});
  }
};
