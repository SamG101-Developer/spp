module;
#include <mp++/integer.hpp>
#include <mp++/rational.hpp>

export module mppp;


export namespace mppp {
    using BigInt = mppp::integer<1>;
    using BigDec = mppp::rational<1>;
    using ::mppp::cmp;

    template <typename T, typename U>
    auto operator<(T const &a, U const &b) -> bool {
        return ::mppp::detail::dispatch_less_than(a, b);
    }

    template <typename T, typename U>
    auto operator>(T const &a, U const &b) -> bool {
        return ::mppp::detail::dispatch_greater_than(a, b);
    }
}
