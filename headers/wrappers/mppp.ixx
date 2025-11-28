module;
#include <mp++/integer.hpp>
#include <mp++/rational.hpp>

export module mppp;


export namespace mppp {
    using BigInt = mppp::integer<1>;
    using BigDec = mppp::rational<1>;
}
