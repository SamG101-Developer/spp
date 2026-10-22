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
  auto SignedBounds(
    const std::size_t bits) -> spp::Pair<numex::BigInt, numex::BigInt> {
    // The signed bounds use the negative and positive for
    // "2 ** (n - 1)" where "n" is the number of bits.
    return spp::MakePair(
      -(numex::BigInt(1) << (bits - 1)), (numex::BigInt(1) << (bits - 1)) - 1);
  }

  /// The range of an unsigned integer "bits" wide.
  auto UnsignedBounds(
    const std::size_t bits) -> spp::Pair<numex::BigInt, numex::BigInt> {
    // The unsigned bounds use the positive for "2 ** n"
    // where "n" is the number of bits (no negative).
    return spp::MakePair(
      numex::BigInt(0), (numex::BigInt(1) << bits) - 1);
  }

  /// The range of an IEEE float with "digits" significand bits
  /// and a largest exponent of "max_exp": its largest finite
  /// magnitude, either sign.
  auto IeeeBounds(
    const std::size_t digits, const std::size_t max_exp)
    -> spp::Pair<numex::BigDec, numex::BigDec> {
    // Key the float bounds off of the magnitude, calculated by
    // bit-shifting with the number of bits and the exponent.
    const auto magnitude = (((numex::BigInt(1) << digits) - 1) << (max_exp - digits)).ToString();
    return spp::MakePair(numex::BigDec(("-" + magnitude).c_str()), numex::BigDec(magnitude.c_str()));
  }
}

/// [CHECKED]
auto spp::utils::numbers::IntegerBounds() -> IntLimitMap const& {
  // Return a reference to a statically created integer
  // bounds limit map.
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

/// [CHECKED]
auto spp::utils::numbers::FloatBounds() -> FloatLimitMap const& {
  // Return a reference to a statically created float
  // bounds limit map.
  static const auto bounds = FloatLimitMap{
    {Str("f8"), MakePair(numex::BigDec("-448"), numex::BigDec("448"))},
    {Str("f16"), IeeeBounds(11, 16)},
    {Str("f32"), IeeeBounds(24, 128)},
    {Str("f64"), IeeeBounds(53, 1024)},
    {Str("f128"), IeeeBounds(113, 16384)}
  };
  return bounds;
}

/// [CHECKED]
auto spp::utils::numbers::WrapToInteger(
  numex::BigInt const &value,
  Str const &type)
  -> Opt<numex::BigInt> {
  // If the type doesn't match, then return nullopt, otherwise
  // take the bounds from the map, keyed or the type.
  const auto bounds = IntegerBounds().find(type);
  if (bounds == IntegerBounds().end()) { return Nullopt; }
  auto const &[lower, upper] = bounds->second;

  // Calculate the modulus against the range of the integer
  // bounds, wrapping the value to the bounds its type should
  // fit within.
  const auto modulus = upper - lower + numex::BigInt(1);
  auto wrapped = value % modulus;
  if (wrapped.IsNegative()) { wrapped = wrapped + modulus; }
  if (wrapped > upper) { wrapped = wrapped - modulus; }
  return wrapped;
}

auto spp::utils::numbers::ShiftRightLogical(
  numex::BigInt const &value, const std::uint32_t count,
  Str const &type) -> Opt<numex::BigInt> {
  // Read the value as its type's bits (a negative one is 2^w
  // above itself), shift those, and read the result back.
  const auto bounds = IntegerBounds().find(type);
  if (bounds == IntegerBounds().end()) { return Nullopt; }
  auto const &[lower, upper] = bounds->second;
  const auto modulus = upper - lower + numex::BigInt(1);
  auto bits = value % modulus;
  if (bits.IsNegative()) { bits = bits + modulus; }
  return WrapToInteger(bits >> count, type);
}

auto spp::utils::numbers::IsIeeeFloat(
  Str const &type) -> bool {
  return type == "f32" or type == "f64";
}

auto spp::utils::numbers::ReadIeeeFloat(
  const StrView text, Str const &type) -> double {
  // "from_chars" rounds to nearest, as a literal is read at run
  // time.
  if (type == "f32") {
    auto value = 0.0f;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return static_cast<double>(value);
  }
  auto value = 0.0;
  std::from_chars(text.data(), text.data() + text.size(), value);
  return value;
}

auto spp::utils::numbers::ReadIeeeFloat(
  numex::BigDec const &value, Str const &type) -> double {
  // Every value of the type is a multiple of its smallest subnormal, so its decimal ends by that many places.
  const auto places = type == "f32" ? 149u : 1074u;
  const auto magnitude = (value.IsNegative() ? -value : value).Decimal(places);
  return ReadIeeeFloat((value.IsNegative() ? "-" : "") + magnitude, type);
}

auto spp::utils::numbers::WriteIeeeFloat(
  const double value, Str const &type) -> Opt<Str> {
  // One rounding to the type: arithmetic done in a "double" and
  // rounded once to an "f32" is the "f32" result.
  const auto rounded = type == "f32" ? static_cast<double>(static_cast<float>(value)) : value;
  if (not std::isfinite(rounded)) { return Nullopt; }
  auto buffer = Arr<char, 1100>();
  const auto end = type == "f32"
    ? std::to_chars(buffer.data(), buffer.data() + buffer.size(), static_cast<float>(value), std::chars_format::fixed)
    : std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::fixed);
  return Str(buffer.data(), end.ptr);
}

/// [CHECKED]
auto spp::utils::numbers::ToU64(
  numex::BigInt const &value)
  -> Opt<std::uint64_t> {
  // Within the "u64" range, read back through its text (the
  // big integer has no native conversion).
  auto const &[lower, upper] = IntegerBounds().at("u64");
  if (value < lower or value > upper) { return Nullopt; }
  return static_cast<std::uint64_t>(std::stoull(value.ToString()));
}
