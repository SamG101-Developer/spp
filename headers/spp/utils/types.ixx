module;
#include <spp/macros.hpp>
#include <version>

export module spp.utils.types;
import ankerl;
import std;
import genex.span;

namespace spp {
  SPP_EXP_CLS template <typename T>
  using Shared = std::shared_ptr<T>;

  SPP_EXP_CLS template <typename T>
  using Weak = std::weak_ptr<T>;

  SPP_EXP_CLS template <typename T>
  using Unique = std::unique_ptr<T>;

  SPP_EXP_CLS template <typename T, typename A = std::allocator<T>>
  using Vec = std::vector<T, A>;

  SPP_EXP_CLS template <typename T, std::size_t N>
  using Arr = std::array<T, N>;

  SPP_EXP_CLS template <typename T>
  using Opt = std::optional<T>;

  SPP_EXP_CON inline constexpr auto Nullopt = std::nullopt;

  SPP_EXP_CLS
  using Path = std::filesystem::path;

  SPP_EXP_CLS template <typename T, typename Enable = void>
  using Hash = ankerl::unordered_dense::hash<T, Enable>;

  SPP_EXP_CLS template <typename K, typename V, typename H=Hash<K>, typename Eq=std::equal_to<K>, typename A=std::allocator<std::pair<K, V>>>
  using Map = ankerl::unordered_dense::map<K, V, H, Eq, A>;

  SPP_EXP_CLS template <typename T, typename H=Hash<T>, typename Eq=std::equal_to<T>, typename A=std::allocator<T>>
  using Set = ankerl::unordered_dense::set<T, H, Eq, A>;

  SPP_EXP_CLS template <typename K, typename V, typename H=Hash<K>, typename Eq=std::equal_to<K>, typename A=std::allocator<std::pair<K, V>>>
  using StableMap = ankerl::unordered_dense::segmented_map<K, V, H, Eq, A>;

  SPP_EXP_CLS template <typename T, typename H=Hash<T>, typename Eq=std::equal_to<T>, typename A=std::allocator<T>>
  using StableSet = ankerl::unordered_dense::segmented_set<T, H, Eq, A>;

  SPP_EXP_CLS template <typename K, typename V>
  using OrderedMap = std::map<K, V>;

  SPP_EXP_CLS
  using Str = std::string; // stringzilla

  SPP_EXP_CLS
  using StrView = std::string_view; // stringzilla

  SPP_EXP_CLS
  using Ordering = std::strong_ordering;

  export template <typename... Ts>
  using Tup = std::tuple<Ts...>;

  export template <typename K, typename V>
  using Pair = std::pair<K, V>;

  SPP_EXP_CLS template <typename T>
  using EnableLocalSharedFromThis = std::enable_shared_from_this<T>;

  template <typename Sig>
  struct _IsFunctionSignature : std::false_type {};

  template <typename R, typename... Args>
  struct _IsFunctionSignature<R(Args...)> : std::true_type {};

  template <typename Sig>
  concept IsFunctionSignature = _IsFunctionSignature<Sig>::value;

  template <typename Sig>
  struct _FuncOf;

  template <typename R, typename... Args>
  struct _FuncOf<R(Args...)> {
#if defined(__cpp_lib_copyable_function)
    using type = std::copyable_function<R(Args...) const>;
#else
    using type = std::function<R(Args...)>;
#endif
  };

  SPP_EXP_CLS template <typename Sig> requires IsFunctionSignature<Sig>
  using Func = typename _FuncOf<Sig>::type;

  SPP_EXP_CLS template <typename Sig> requires IsFunctionSignature<Sig>
#if defined(__cpp_lib_function_ref)
  using FuncRef = std::function_ref<Sig>;
#else
  using FuncRef = Func<Sig>;
#endif

  SPP_EXP_CLS template <typename T>
  using Span = genex::span<T>;

  SPP_EXP_CLS template <typename... Ts>
  using Variant = std::variant<Ts...>;

  SPP_EXP_CLS
  using Monostate = std::monostate;

  // The standard functions over tuples and variants, under their
  // own names, so call sites name "spp" and not "std".
  SPP_EXP_FUN using std::get;
  SPP_EXP_FUN using std::get_if;
  SPP_EXP_FUN using std::holds_alternative;
  SPP_EXP_FUN using std::visit;
  SPP_EXP_FUN using std::apply;
  SPP_EXP_FUN using std::tie;
  SPP_EXP_FUN using std::tuple_cat;
  SPP_EXP_FUN using std::forward_as_tuple;
  SPP_EXP_FUN using std::make_from_tuple;
  SPP_EXP_FUN using std::ignore;

  SPP_EXP_FUN template <typename K, typename V>
  SPP_ATTR_ALWAYS_INLINE SPP_ATTR_HOT inline auto MakePair(
    K &&key, V &&value) -> Pair<std::decay_t<K>, std::decay_t<V>> {
    return Pair<std::decay_t<K>, std::decay_t<V>>(std::forward<K>(key), std::forward<V>(value));
  }

  SPP_EXP_FUN template <typename... Args>
  SPP_ATTR_ALWAYS_INLINE SPP_ATTR_HOT inline auto MakeTuple(
    Args &&... args) -> Tup<std::decay_t<Args>...> {
    return Tup<std::decay_t<Args>...>(std::forward<Args>(args)...);
  }

  SPP_EXP_FUN template <typename T, typename... Args> requires std::constructible_from<T, Args...>
  SPP_ATTR_ALWAYS_INLINE SPP_ATTR_HOT inline auto MakeShared(Args &&... args) -> Shared<T> {
    return std::make_shared<T>(std::forward<Args>(args)...);
  }

  SPP_EXP_FUN template <typename T, typename... Args> requires std::constructible_from<T, Args...>
  SPP_ATTR_ALWAYS_INLINE SPP_ATTR_HOT inline auto MakeUnique(Args &&... args) -> Unique<T> {
    return std::make_unique<T>(std::forward<Args>(args)...);
  }

  SPP_EXP_FUN template <typename T, typename... Args> requires std::constructible_from<T, Args...>
  SPP_ATTR_ALWAYS_INLINE SPP_ATTR_HOT inline auto MakeUniqueAndRaw(Args &&... args) -> Pair<Unique<T>, T*> {
    auto unique = MakeUnique<T>(std::forward<Args>(args)...);
    return {std::move(unique), unique.get()};
  }

  SPP_EXP_FUN SPP_ATTR_ALWAYS_INLINE
  inline auto operator""_str(const char *str, const std::size_t len) -> Str {
    return Str(str, len);
  }

  SPP_EXP_FUN SPP_ATTR_ALWAYS_INLINE
  inline auto operator""_str_view(const char *str, const std::size_t len) -> StrView {
    return StrView(str, len);
  }
}
