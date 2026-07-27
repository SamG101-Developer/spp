module;
#include <spp/macros.hpp>
#ifdef __clang__
#include <bits/floatn-common.h>
#endif

export module spp.utils.types;
import std;

#ifdef __clang__
export namespace std {
  using float16_t = _Float16;
  using float32_t = _Float32;
  using float64_t = _Float64;
  using float128_t = __float128;
}
#endif

namespace spp {
  SPP_EXP_CLS
  template <typename T>
  using Shared = std::shared_ptr<T>;

  SPP_EXP_CLS
  template <typename T>
  using Weak = std::weak_ptr<T>;

  SPP_EXP_CLS
  template <typename T>
  using Unique = std::unique_ptr<T>;

  SPP_EXP_CLS
  template <typename T, typename A = std::allocator<T>>
  class Vec;

  SPP_EXP_CLS
  using Str = std::string; // stringzilla::string;

  SPP_EXP_CLS
  using StrView = std::string_view; // stringzilla::string_view;

  SPP_EXP_CLS
  template <typename T, typename A = std::allocator<Shared<T>>>
  using SharedVec = Vec<Shared<T>, A>;

  SPP_EXP_CLS
  template <typename T, typename A = std::allocator<Unique<T>>>
  using UniqueVec = Vec<Unique<T>, A>;

  SPP_EXP_CLS
  using Ordering = std::strong_ordering;

  SPP_EXP_CLS
  template <typename K, typename V>
  struct Pair;

  SPP_EXP_CLS
  template <typename T>
  using EnableLocalSharedFromThis = std::enable_shared_from_this<T>;

  template <typename Sig>
  struct _IsFunctionSignature : std::false_type {};

  template <typename R, typename... Args>
  struct _IsFunctionSignature<R(Args...)> : std::true_type {};

  template <typename Sig>
  concept IsFunctionSignature = _IsFunctionSignature<Sig>::value;

  SPP_EXP_CLS
  template <typename Sig>
    requires IsFunctionSignature<Sig>
  using Function = std::function<Sig>;

  SPP_EXP_CLS
  template <typename Sig>
    requires IsFunctionSignature<Sig>
  using FunctionRef = std::function_ref<Sig>;

  SPP_EXP_FUN

  template <typename T, typename... Args>
  SPP_ATTR_ALWAYS_INLINE SPP_ATTR_HOT inline auto MakeShared(Args &&... args) -> Shared<T> {
    return std::make_shared<T>(std::forward<Args>(args)...);
  }

  SPP_EXP_FUN

  template <typename T, typename... Args>
  SPP_ATTR_ALWAYS_INLINE SPP_ATTR_HOT inline auto MakeUnique(Args &&... args) -> Unique<T> {
    return std::make_unique<T>(std::forward<Args>(args)...);
  }

  SPP_EXP_FUN

  template <typename K, typename V>
  SPP_ATTR_ALWAYS_INLINE SPP_ATTR_HOT inline auto MakePair(K &&key,
    V &&value) -> Pair<std::decay_t<K>, std::decay_t<V>> {
    return Pair<std::decay_t<K>, std::decay_t<V>>(std::forward<K>(key), std::forward<V>(value));
  }

  SPP_EXP_FUN
  SPP_ATTR_ALWAYS_INLINE inline auto operator""_str(const char *str, const std::size_t len) -> Str {
    return Str(str, len);
  }

  SPP_EXP_FUN
  SPP_ATTR_ALWAYS_INLINE inline auto operator""_str_view(const char *str, const std::size_t len) -> StrView {
    return StrView(str, len);
  }

  // Deduction guide for iterator constructor
  template <typename I, typename A = std::allocator<std::iter_value_t<I>>> requires std::input_iterator<I>
  Vec(I, I, A const & = A()) -> Vec<std::iter_value_t<I>, A>;

  // Deduction guide for variadic arguments / init list
  template <typename T, typename A = std::allocator<T>>
  Vec(std::initializer_list<T>, A const & = A()) -> Vec<T, A>;
}

SPP_EXP_CLS

template <typename T, typename A>
class spp::Vec {
public:
  friend auto operator==(const Vec &a, const Vec &b) -> bool { return a._Vec == b._Vec; }
  friend auto operator!=(const Vec &a, const Vec &b) -> bool { return a._Vec != b._Vec; }
  friend auto operator<=(const Vec &a, const Vec &b) -> bool { return a._Vec <= b._Vec; }
  friend auto operator>=(const Vec &a, const Vec &b) -> bool { return a._Vec >= b._Vec; }
  friend auto operator<(const Vec &a, const Vec &b) -> bool { return a._Vec < b._Vec; }
  friend auto operator>(const Vec &a, const Vec &b) -> bool { return a._Vec > b._Vec; }

  using underlying_type = std::vector<T, A>;
  using allocator_type = underlying_type::allocator_type;
  using value_type = underlying_type::value_type;
  using pointer = underlying_type::pointer;
  using const_pointer = underlying_type::const_pointer;
  using size_type = underlying_type::size_type;
  using difference_type = underlying_type::difference_type;
  using reference = underlying_type::reference;
  using const_reference = underlying_type::const_reference;
  using iterator = underlying_type::iterator;
  using const_iterator = underlying_type::const_iterator;
  using reverse_iterator = underlying_type::reverse_iterator;
  using const_reverse_iterator = underlying_type::const_reverse_iterator;
  using iterator_category = std::random_access_iterator_tag;
  using iterator_concept = std::random_access_iterator_tag;

  constexpr Vec() = default;
  explicit constexpr Vec(A const &allocator) noexcept : _Vec(allocator) {}
  explicit constexpr Vec(std::size_t n, A const &allocator = A()) : _Vec(n, allocator) {}
  constexpr Vec(std::size_t n, T const &v, A const &allocator = A()) : _Vec(n, v, allocator) {}
  constexpr Vec(std::initializer_list<T> list, A const &allocator = A()) : _Vec(list, allocator) {}

  template <typename I> requires std::input_iterator<I>
  Vec(I first, I last, A const &allocator = A()) : _Vec(first, last, allocator) {}

  Vec(Vec const &other, A const &allocator = A()) : _Vec(other._Vec, allocator) {}
  Vec(Vec &&other, A const &allocator = A()) noexcept : _Vec(std::move(other._Vec), allocator) {}
  ~Vec() = default;

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto IsEmpty() const noexcept -> bool { return _Vec.empty(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto Len() const noexcept -> std::size_t { return _Vec.size(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto MaxSize() const noexcept -> std::size_t { return _Vec.max_size(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto Cap() const noexcept -> std::size_t { return _Vec.capacity(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto Data() noexcept -> pointer { return _Vec.data(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto Data() const noexcept -> const_pointer { return _Vec.data(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto operator[](std::size_t idx) -> reference { return _Vec[idx]; }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto operator[](std::size_t idx) const noexcept -> const_reference { return _Vec[idx]; }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto At(std::size_t idx) -> T& { return _Vec.at(idx); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto At(std::size_t idx) const -> T const& { return _Vec.at(idx); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto Front() -> reference { return _Vec.front(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto Front() const -> const_reference { return _Vec.front(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto Back() -> reference { return _Vec.back(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto Back() const noexcept -> const_reference { return _Vec.back(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto begin() noexcept -> iterator { return _Vec.begin(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto begin() const noexcept -> const_iterator { return _Vec.begin(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto end() noexcept -> iterator { return _Vec.end(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto end() const noexcept -> const_iterator { return _Vec.end(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto cbegin() const noexcept -> const_iterator { return _Vec.cbegin(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto cend() const noexcept -> const_iterator { return _Vec.cend(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto rbegin() noexcept -> reverse_iterator { return _Vec.rbegin(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto rbegin() const noexcept -> const_reverse_iterator { return _Vec.rbegin(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto rend() noexcept -> reverse_iterator { return _Vec.rend(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto rend() const noexcept -> const_reverse_iterator { return _Vec.rend(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto crbegin() const noexcept -> const_reverse_iterator { return _Vec.crbegin(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto crend() const noexcept -> const_reverse_iterator { return _Vec.crend(); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto GetAllocator() const -> allocator_type { return _Vec.get_allocator(); }

  auto operator=(std::initializer_list<value_type> list) -> Vec& {
    _Vec = list;
    return *this;
  }

  auto operator=(Vec const &other) -> Vec& = default;
  auto operator=(Vec &&other) noexcept -> Vec& = default;

  SPP_ATTR_ALWAYS_INLINE
  auto Assign(size_type n, const_reference v) { _Vec.assign(n, v); }

  SPP_ATTR_ALWAYS_INLINE
  auto Assign(std::initializer_list<value_type> list) { _Vec.assign(list); }

  template <typename I> requires std::input_iterator<I>
  SPP_ATTR_ALWAYS_INLINE
  auto Assign(I first, I last) { _Vec.assign(first, last); }

  SPP_ATTR_ALWAYS_INLINE
  auto Resize(size_type n) { _Vec.resize(n); }

  SPP_ATTR_ALWAYS_INLINE
  auto Resize(size_type n, const_reference v) { _Vec.resize(n, v); }

  SPP_ATTR_ALWAYS_INLINE
  auto Insert(const_iterator pos, const_reference v) { _Vec.insert(pos, v); }

  SPP_ATTR_ALWAYS_INLINE
  auto Insert(const_iterator pos, value_type &&v) { _Vec.insert(pos, std::move(v)); }

  SPP_ATTR_ALWAYS_INLINE
  auto Insert(const_iterator pos, size_type n, const_reference v) { _Vec.insert(pos, n, v); }

  SPP_ATTR_ALWAYS_INLINE
  auto Insert(const_iterator pos, std::initializer_list<value_type> list) { _Vec.insert(pos, list); }

  template <typename I> requires std::input_iterator<I>
  SPP_ATTR_ALWAYS_INLINE
  auto Insert(const_iterator pos, I first, I last) { _Vec.insert(pos, first, last); }

  template <typename... Args>
  SPP_ATTR_ALWAYS_INLINE
  // requires std::constructible_from<T, Args...>
  auto Emplace(const_iterator pos, Args &&... args) { _Vec.emplace(pos, std::forward<Args>(args)...); }

  template <typename... Args>
  SPP_ATTR_ALWAYS_INLINE
  // requires std::constructible_from<T, Args...>
  auto EmplaceBack(Args &&... args) { _Vec.emplace_back(std::forward<Args>(args)...); }

  SPP_ATTR_ALWAYS_INLINE
  auto PushBack(const_reference v) { _Vec.push_back(v); }

  SPP_ATTR_ALWAYS_INLINE
  auto PushBack(value_type &&v) { _Vec.push_back(std::move(v)); }

  SPP_ATTR_ALWAYS_INLINE
  auto AppendRange(Vec const &other) { _Vec.insert(_Vec.end(), other._Vec.begin(), other._Vec.end()); }

  SPP_ATTR_ALWAYS_INLINE
  auto AppendRange(Vec &&other) {
    _Vec.insert(_Vec.end(), std::make_move_iterator(other._Vec.begin()), std::make_move_iterator(other._Vec.end()));
  }

  template <typename R>
    requires (std::ranges::range<R>
      and std::constructible_from<value_type, std::ranges::range_value_t<R>>
    )
  SPP_ATTR_ALWAYS_INLINE
  auto AppendRange(R &&range) {
    _Vec.insert(_Vec.end(), std::make_move_iterator(std::ranges::begin(range)),
                std::make_move_iterator(std::ranges::end(range)));
  }

  SPP_ATTR_ALWAYS_INLINE
  auto PopBack() -> void { _Vec.pop_back(); }

  SPP_ATTR_ALWAYS_INLINE
  auto Erase(const_iterator pos) { _Vec.erase(pos); }

  SPP_ATTR_ALWAYS_INLINE
  auto Erase(const_iterator first, const_iterator last) { _Vec.erase(first, last); }

  SPP_ATTR_ALWAYS_INLINE
  auto Clear() noexcept { _Vec.clear(); }

  SPP_ATTR_ALWAYS_INLINE
  auto Reserve(size_type new_cap) { _Vec.reserve(new_cap); }

  SPP_ATTR_ALWAYS_INLINE
  auto ShrinkToFit() { _Vec.shrink_to_fit(); }

  SPP_ATTR_ALWAYS_INLINE
  auto Swap(Vec &other) noexcept { _Vec.swap(other._Vec); }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto ToStdVector() const noexcept -> std::vector<value_type> {
    return std::vector<value_type, allocator_type>(_Vec.begin(), _Vec.end());
  }

  SPP_ATTR_NODISCARD SPP_ATTR_ALWAYS_INLINE
  auto ToView() const noexcept -> std::span<const value_type> {
    return _Vec;
  }

  // SPP_ATTR_ALWAYS_INLINE
  // auto ToLlvmArrayRef() noexcept -> llvm::ArrayRef<value_type> {
  //     return llvm::ArrayRef<value_type>(_Vec.begin(), _Vec.end());
  // }

  // Legacy needed for ranges
  SPP_ATTR_ALWAYS_INLINE auto push_back(value_type const &v) -> void { _Vec.push_back(v); }
  SPP_ATTR_ALWAYS_INLINE auto push_back(value_type &&v) -> void { _Vec.push_back(std::move(v)); }

  SPP_ATTR_ALWAYS_INLINE auto insert(const_iterator pos, value_type const &v) -> iterator {
    return _Vec.insert(pos, v);
  }

  SPP_ATTR_ALWAYS_INLINE auto reserve(size_type new_cap) -> void { _Vec.reserve(new_cap); }
  SPP_ATTR_ALWAYS_INLINE auto size() const noexcept { return _Vec.size(); }
  SPP_ATTR_ALWAYS_INLINE auto erase(const_iterator pos) -> iterator { return _Vec.erase(pos); }

  SPP_ATTR_ALWAYS_INLINE auto erase(const_iterator first, const_iterator last) -> iterator {
    return _Vec.erase(first, last);
  }

private:
  underlying_type _Vec;
};

SPP_EXP_CLS

template <typename K, typename V>
struct spp::Pair {
  K First;
  V Second;

  Pair() = default;
  Pair(Pair const &) = default;
  Pair(Pair &&) noexcept = default;
  auto operator=(Pair const &) -> Pair& = default;
  auto operator=(Pair &&) noexcept -> Pair& = default;

  template <typename K2, typename V2>
    requires std::constructible_from<K, K2 const&> && std::constructible_from<V, V2 const&>
  explicit Pair(Pair<K2, V2> const &other)
    noexcept(std::is_nothrow_constructible_v<K, K2 const&> && std::is_nothrow_constructible_v<V, V2 const&>) :
    First(other.First), Second(other.Second) {}

  template <typename K2, typename V2>
    requires std::constructible_from<K, K2&&> && std::constructible_from<V, V2&&>
  explicit Pair(Pair<K2, V2> &&other)
    noexcept(std::is_nothrow_constructible_v<K, K2&&> && std::is_nothrow_constructible_v<V, V2&&>) :
    First(std::move(other.First)), Second(std::move(other.Second)) {}

  template <typename K2, typename V2>
    requires std::assignable_from<K&, K2 const&> && std::assignable_from<V&, V2 const&>
  auto operator=(Pair<K2, V2> const &other)
    noexcept(std::is_nothrow_assignable_v<K, K2 const&> && std::is_nothrow_assignable_v<V, V2 const&>) -> Pair& {
    First = other.First;
    Second = other.Second;
    return *this;
  }

  template <typename K2, typename V2>
    requires std::assignable_from<K&, K2&&> && std::assignable_from<V&, V2&&>
  auto operator=(Pair<K2, V2> &&other)
    noexcept(std::is_nothrow_assignable_v<K, K2&&> && std::is_nothrow_assignable_v<V, V2&&>) -> Pair& {
    First = std::move(other.First);
    Second = std::move(other.Second);
    return *this;
  }

  template <typename K2, typename V2>
    requires std::convertible_to<K2, K> && std::convertible_to<V2, V>
  Pair(K2 &&key, V2 &&value)
    noexcept(std::is_nothrow_constructible_v<K, K2&&> && std::is_nothrow_constructible_v<V, V2&&>) :
    First(std::forward<K2>(key)), Second(std::forward<V2>(value)) {}

  template <typename K2, typename V2>
    requires std::constructible_from<K, K2 const&> && std::constructible_from<V, V2 const&>
  operator std::pair<K2, V2>() const
    noexcept(std::is_nothrow_constructible_v<K2, K const&> && std::is_nothrow_constructible_v<V2, V const&>) {
    return std::pair<K2, V2>(First, Second);
  }

  template <typename K2, typename V2>
    requires std::constructible_from<K, K2&&> && std::constructible_from<V, V2&&>
  operator std::pair<K2, V2>() && noexcept(std::is_nothrow_constructible_v<K2, K&&> && std::is_nothrow_constructible_v<
    V2, V&&>) {
    return std::pair<K2, V2>(std::move(First), std::move(Second));
  }

  auto operator==(Pair const &other) const -> bool {
    return First == other.First && Second == other.Second;
  }
};
