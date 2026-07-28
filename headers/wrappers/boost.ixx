module;
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_int/bitwise.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

#define SPECIALIZE_OP(OP, NAME)                                                                                  \
  template <> struct OP<boost::int128_t> {                                                                       \
    using result_type = boost::int128_t;                                                                         \
    using first_argument_type = boost::int128_t;                                                                 \
    using second_argument_type = boost::int128_t;                                                                \
    boost::int128_t operator()(const boost::int128_t& a, const boost::int128_t& b) const { return a NAME b; }    \
  };                                                                                                             \
  template <> struct OP<boost::uint128_t> {                                                                      \
    using result_type = boost::uint128_t;                                                                        \
    using first_argument_type = boost::uint128_t;                                                                \
    using second_argument_type = boost::uint128_t;                                                               \
    boost::uint128_t operator()(const boost::uint128_t& a, const boost::uint128_t& b) const { return a NAME b; } \
  };                                                                                                             \
  template <> struct OP<boost::int256_t> {                                                                       \
    using result_type = boost::int256_t;                                                                         \
    using first_argument_type = boost::int256_t;                                                                 \
    using second_argument_type = boost::int256_t;                                                                \
    boost::int256_t operator()(const boost::int256_t& a, const boost::int256_t& b) const { return a NAME b; }    \
  };                                                                                                             \
  template <> struct OP<boost::uint256_t> {                                                                      \
    using result_type = boost::uint256_t;                                                                        \
    using first_argument_type = boost::uint256_t;                                                                \
    using second_argument_type = boost::uint256_t;                                                               \
    boost::uint256_t operator()(const boost::uint256_t& a, const boost::uint256_t& b) const { return a NAME b; } \
  };

#define SPECIALIZE_SHIFT_OP(OP, NAME)                                                         \
  template <> struct OP<boost::int128_t> {                                                    \
    using result_type = boost::int128_t;                                                      \
    using first_argument_type = boost::int128_t;                                              \
    using second_argument_type = boost::int128_t;                                             \
    boost::int128_t operator()(const boost::int128_t& a, const boost::int128_t& b) const {    \
      return a NAME static_cast<unsigned>(b);                                                 \
    }                                                                                         \
  };                                                                                          \
  template <> struct OP<boost::uint128_t> {                                                   \
    using result_type = boost::uint128_t;                                                     \
    using first_argument_type = boost::uint128_t;                                             \
    using second_argument_type = boost::uint128_t;                                            \
    boost::uint128_t operator()(const boost::uint128_t& a, const boost::uint128_t& b) const { \
      return a NAME static_cast<unsigned>(b);                                                 \
    }                                                                                         \
  };                                                                                          \
  template <> struct OP<boost::int256_t> {                                                    \
    using result_type = boost::int256_t;                                                      \
    using first_argument_type = boost::int256_t;                                              \
    using second_argument_type = boost::int256_t;                                             \
    boost::int256_t operator()(const boost::int256_t& a, const boost::int256_t& b) const {    \
      return a NAME static_cast<unsigned>(b);                                                 \
    }                                                                                         \
  };                                                                                          \
  template <> struct OP<boost::uint256_t> {                                                   \
    using result_type = boost::uint256_t;                                                     \
    using first_argument_type = boost::uint256_t;                                             \
    using second_argument_type = boost::uint256_t;                                            \
    boost::uint256_t operator()(const boost::uint256_t& a, const boost::uint256_t& b) const { \
      return a NAME static_cast<unsigned>(b);                                                 \
    }                                                                                         \
  };

export module boost;
import spp.utils.types;

export namespace boost {
  using BigInt = ::boost::multiprecision::cpp_int;
  using BigDec = ::boost::multiprecision::cpp_dec_float_100;

  using ::boost::multiprecision::int128_t;
  using ::boost::multiprecision::uint128_t;
  using ::boost::multiprecision::int256_t;
  using ::boost::multiprecision::uint256_t;

  // DO NOT REMOVE ANY OF THESE (BOOST INTERNAL USAGE)
  using ::boost::multiprecision::backends::cpp_int_backend;
  using ::boost::multiprecision::backends::eval_right_shift;
  using ::boost::multiprecision::backends::eval_get_sign;
  using ::boost::multiprecision::backends::eval_complement;
  using ::boost::multiprecision::backends::eval_ldexp;
  using ::boost::multiprecision::backends::eval_frexp;
  using ::boost::multiprecision::backends::eval_multiply;
  using ::boost::multiprecision::backends::eval_add;
  using ::boost::multiprecision::backends::divide_unsigned_helper;

  using ::boost::multiprecision::number_category;
  using ::boost::multiprecision::number_kind_integer;
  using ::boost::multiprecision::number_kind_floating_point;
}

export using ::boost::multiprecision::operator-;

SPECIALIZE_OP(std::plus, +)
SPECIALIZE_OP(std::minus, -)
SPECIALIZE_OP(std::multiplies, *)
SPECIALIZE_OP(std::divides, /)
SPECIALIZE_OP(std::modulus, %)
SPECIALIZE_OP(std::less, <);
SPECIALIZE_OP(std::greater, >);
SPECIALIZE_OP(std::less_equal, <=)
SPECIALIZE_OP(std::greater_equal, >=)
SPECIALIZE_OP(std::bit_and, &)
SPECIALIZE_OP(std::bit_or, |)
SPECIALIZE_OP(std::bit_xor, ^)
SPECIALIZE_SHIFT_OP(std::bit_shl, <<)
SPECIALIZE_SHIFT_OP(std::bit_shr, >>)
