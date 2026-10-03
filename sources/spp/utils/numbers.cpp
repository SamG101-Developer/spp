module;
#include <spp/macros.hpp>

module spp.utils.numbers;
import spp.utils.types;
import std;
import sys;
import numex.big_dec;
import numex.big_int;

namespace {
  /// The range of a signed integer "bits" wide.
  auto SignedBounds(const std::size_t bits) -> spp::Pair<numex::BigInt, numex::BigInt> {
    return spp::MakePair(-(numex::BigInt(1) << (bits - 1)), (numex::BigInt(1) << (bits - 1)) - 1);
  }

  /// The range of an unsigned integer "bits" wide.
  auto UnsignedBounds(const std::size_t bits) -> spp::Pair<numex::BigInt, numex::BigInt> {
    return spp::MakePair(numex::BigInt(0), (numex::BigInt(1) << bits) - 1);
  }

  /// The range of an IEEE float with "digits" significand bits and a largest exponent of "max_exp": its largest finite
  /// magnitude, either sign.
  auto IeeeBounds(const std::size_t digits, const std::size_t max_exp) -> spp::Pair<numex::BigDec, numex::BigDec> {
    const auto magnitude = (((numex::BigInt(1) << digits) - 1) << (max_exp - digits)).ToString();
    return spp::MakePair(numex::BigDec(("-" + magnitude).c_str()), numex::BigDec(magnitude.c_str()));
  }
}

auto spp::utils::numbers::IntegerBounds()
  -> IntLimitMap const& {
  static const auto bounds = IntLimitMap{
    {Str("s8"), SignedBounds(8)},
    {Str("s16"), SignedBounds(16)},
    {Str("s32"), SignedBounds(32)},
    {Str("s64"), SignedBounds(64)},
    {Str("s128"), SignedBounds(128)},
    {Str("s256"), SignedBounds(256)},
    {Str("sz"), SignedBounds(sizeof(sys::ssize_t) * 8)},
    {Str("u8"), UnsignedBounds(8)},
    {Str("u16"), UnsignedBounds(16)},
    {Str("u32"), UnsignedBounds(32)},
    {Str("u64"), UnsignedBounds(64)},
    {Str("u128"), UnsignedBounds(128)},
    {Str("u256"), UnsignedBounds(256)},
    {Str("uz"), UnsignedBounds(sizeof(std::size_t) * 8)},
  };
  return bounds;
}

auto spp::utils::numbers::FloatBounds()
  -> FloatLimitMap const& {
  static const auto bounds = FloatLimitMap{
    {Str("f8"), MakePair(numex::BigDec("-448"), numex::BigDec("448"))},
    {Str("f16"), IeeeBounds(11, 16)},
    {Str("f32"), IeeeBounds(24, 128)},
    {Str("f64"), IeeeBounds(53, 1024)},
    {Str("f128"), IeeeBounds(113, 16384)}
  };
  return bounds;
}
