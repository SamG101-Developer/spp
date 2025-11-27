module;
#include <spp/macros.hpp>

export module spp.utils.traits;
import std;


namespace spp::utils::traits {
    SPP_EXP_CLS template <typename T>
    struct function_traits : function_traits<decltype(&T::operator())> {
    };

    SPP_EXP_CLS template <std::size_t N, typename F>
    using nth_param_t = function_traits<F>::template arg_type<N>;
}


SPP_EXP_CLS template <typename Ret, typename... Args>
struct spp::utils::traits::function_traits<Ret(*)(Args...)> {
    using ret_t = Ret;
    using func_stl_t = std::function<Ret(Args...)>;
    using func_ptr_t = Ret(*)(Args...);
    using args_t = std::tuple<Args...>;

    static constexpr auto arity = sizeof...(Args);

    template <std::size_t N>
    using arg_t = std::tuple_element_t<N, std::tuple<Args...>>;
};


SPP_EXP_CLS template <typename Class, typename Ret, typename... Args>
struct spp::utils::traits::function_traits<Ret(Class::*)(Args...) const> {
    using ret_t = Ret;
    using func_stl_t = std::function<Ret(Args...)>;
    using func_ptr_t = Ret(*)(Args...);
    using args_t = std::tuple<Args...>;

    static constexpr auto arity = sizeof...(Args);

    template <std::size_t N>
    using arg_t = std::tuple_element_t<N, std::tuple<Args...>>;
};


SPP_EXP_CLS template <typename Class, typename Ret, typename... Args>
struct spp::utils::traits::function_traits<Ret(Class::*)(Args...)> {
    using ret_t = Ret;
    using func_stl_t = std::function<Ret(Args...)>;
    using func_ptr_t = Ret(*)(Args...);
    using args_t = std::tuple<Args...>;

    static constexpr auto arity = sizeof...(Args);

    template <std::size_t N>
    using arg_t = std::tuple_element_t<N, std::tuple<Args...>>;
};
