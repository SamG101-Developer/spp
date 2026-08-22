module;
#include <spp/macros-platforms.hpp>
#include <spp/macros.hpp>

export module spp.utils.traits;
import spp.utils.types;
import boost;
import std;

namespace spp::utils::traits {
  SPP_EXP_CLS template <typename T>
  struct function_traits : function_traits<decltype(&T::operator())> {};

  SPP_EXP_CLS template <std::size_t N, typename F>
  using nth_param_t = function_traits<F>::template arg_type<N>;

  SPP_EXP_CON template <typename T>
  concept integral = std::integral<T>
    || boost::number_category<T>::value == boost::number_kind_integer;

  SPP_EXP_CON template <typename T>
  concept floating_point = std::floating_point<T>
    || boost::number_category<T>::value == boost::number_kind_floating_point
#if SPP_COMPILER_CLANG
    || std::is_same_v<T, std::float16_t>
    || std::is_same_v<T, std::float32_t>
    || std::is_same_v<T, std::float64_t>
    || std::is_same_v<T, std::float128_t>
#endif
  ;
}

SPP_EXP_CLS template <typename Ret, typename... Args>
struct spp::utils::traits::function_traits<Ret(*)(Args...)> {
  using ret_t = Ret;
  using func_stl_t = std::function<Ret(Args...)>;
  using func_ptr_t = Ret(*)(Args...);
  using args_t = Tup<Args...>;

  static constexpr auto arity = sizeof...(Args);

  template <std::size_t N>
  using arg_t = Args...[N];
};

SPP_EXP_CLS template <typename Class, typename Ret, typename... Args>
struct spp::utils::traits::function_traits<Ret(Class::*)(Args...) const> {
  using ret_t = Ret;
  using func_stl_t = std::function<Ret(Args...)>;
  using func_ptr_t = Ret(*)(Args...);
  using args_t = Tup<Args...>;

  static constexpr auto arity = sizeof...(Args);

  template <std::size_t N>
  using arg_t = Args...[N];
};

SPP_EXP_CLS template <typename Class, typename Ret, typename... Args>
struct spp::utils::traits::function_traits<Ret(Class::*)(Args...)> {
  using ret_t = Ret;
  using func_stl_t = std::function<Ret(Args...)>;
  using func_ptr_t = Ret(*)(Args...);
  using args_t = Tup<Args...>;

  static constexpr auto arity = sizeof...(Args);

  template <std::size_t N>
  using arg_t = Args...[N];
};
