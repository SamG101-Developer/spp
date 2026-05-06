module;
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_int/bitwise.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

export module boost;

export namespace boost {
    using BigInt = ::boost::multiprecision::cpp_int;
    using BigDec = ::boost::multiprecision::cpp_dec_float_100;
    using ::boost::multiprecision::pow;

    using ::boost::multiprecision::operator*;
    using ::boost::multiprecision::backends::cpp_int_backend;
    using ::boost::multiprecision::backends::negate_integer;
    using ::boost::multiprecision::backends::eval_right_shift;
    using ::boost::multiprecision::backends::eval_multiply;
    using ::boost::multiprecision::backends::eval_get_sign;
    using ::boost::multiprecision::backends::divide_unsigned_helper;
}
