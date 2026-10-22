module;
#include <spp/macros.hpp>

module spp.utils.numbers;
import spp.utils.ice;
import spp.utils.types;
import llvm;
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

  /// The finite "value" to "digits" significant digits, as a
  /// decimal a literal can hold. "toString" writes a large value
  /// with an exponent, so the point is moved by it instead.
  auto Plain(
    llvm::APFloat const &value, const unsigned digits)
    -> spp::Str {
    using spp::Str;
    auto buffer = llvm::SmallVector<char, 64>();
    value.toString(buffer, digits, std::numeric_limits<unsigned>::max());
    const auto text = Str(buffer.data(), buffer.size());
    const auto e = text.find_first_of("eE");
    if (e == Str::npos) { return text; }

    const auto is_negative = text.starts_with('-');
    const auto mantissa = text.substr(is_negative ? 1 : 0, e - (is_negative ? 1 : 0));
    const auto point = mantissa.find('.');
    const auto int_digits = point == Str::npos ? mantissa : mantissa.substr(0, point);
    const auto all = int_digits + (point == Str::npos ? Str() : mantissa.substr(point + 1));
    const auto at = static_cast<std::int64_t>(int_digits.size()) + std::stoll(text.substr(e + 1));
    const auto sign = Str(is_negative ? "-" : "");
    const auto size = static_cast<std::int64_t>(all.size());
    if (at <= 0) { return sign + "0." + Str(static_cast<std::size_t>(-at), '0') + all; }
    if (at >= size) { return sign + all + Str(static_cast<std::size_t>(at - size), '0'); }
    return sign + all.substr(0, static_cast<std::size_t>(at)) + "." + all.substr(static_cast<std::size_t>(at));
  }

  /// The range of the float "type": its largest finite
  /// magnitude, either sign. Written exactly: it is an integer
  /// of at most 4933 digits (an "f128").
  auto FloatRange(
    spp::Str const &type)
    -> spp::Pair<numex::BigDec, numex::BigDec> {
    const auto largest = llvm::APFloat::getLargest(spp::utils::numbers::FloatSemantics(type));
    const auto exact = numex::BigDec(Plain(largest, 5000));
    return spp::MakePair(-exact, exact);
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
    {Str("f8"), FloatRange("f8")},
    {Str("f16"), FloatRange("f16")},
    {Str("f32"), FloatRange("f32")},
    {Str("f64"), FloatRange("f64")},
    {Str("f128"), FloatRange("f128")}
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

auto spp::utils::numbers::ShiftLeftLogical(
  numex::BigInt const &value, const std::uint32_t count,
  Str const &type) -> Opt<numex::BigInt> {
  // Shift exactly, then read the result back in the type's
  // width, dropping what was shifted past the top.
  return WrapToInteger(value << count, type);
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

auto spp::utils::numbers::FloatSemantics(
  Str const &type) -> llvm::fltSemantics const& {
  // An unsuffixed literal is an "f32".
  if (type == "f8") { return llvm::APFloat::Float8E4M3FN(); }
  if (type == "f16") { return llvm::APFloat::IEEEhalf(); }
  if (type == "f32" or type.empty()) { return llvm::APFloat::IEEEsingle(); }
  if (type == "f64") { return llvm::APFloat::IEEEdouble(); }
  if (type == "f128") { return llvm::APFloat::IEEEquad(); }
  Ice(std::format("no float format for the type \"{}\"", type));
}

auto spp::utils::numbers::FloatDecimalPlaces(
  Str const &type) -> std::uint64_t {
  // The smallest subnormal is 2 to the power of "min_exponent -
  // precision + 1"; a negative power of two has as many decimal
  // places as the magnitude of its exponent.
  auto const &semantics = FloatSemantics(type);
  const auto precision = static_cast<std::int64_t>(llvm::APFloat::semanticsPrecision(semantics));
  return static_cast<std::uint64_t>(precision - 1 - llvm::APFloat::semanticsMinExponent(semantics));
}

auto spp::utils::numbers::ReadFloat(
  const StrView text, Str const &type) -> llvm::APFloat {
  // Rounded to nearest, ties to even, as a literal is read at
  // run time.
  return llvm::APFloat(FloatSemantics(type), llvm::StringRef(text.data(), text.size()));
}

auto spp::utils::numbers::ReadFloat(
  numex::BigDec const &value, Str const &type) -> llvm::APFloat {
  // Every value of the type is a multiple of its smallest
  // subnormal, so its decimal ends by that many places.
  const auto magnitude = (value.IsNegative() ? -value : value).Decimal(FloatDecimalPlaces(type));
  return ReadFloat((value.IsNegative() ? "-" : "") + magnitude, type);
}

auto spp::utils::numbers::WriteFloat(
  llvm::APFloat const &value) -> Opt<Str> {
  if (not value.isFinite()) { return Nullopt; }

  // An integer is written exactly, as "to_chars" writes one, so
  // a result equal to the type's largest value is not read past
  // it ("ValidateBounds" compares exact values). Every float too
  // large for its digits to reach the point is one.
  if (value.isInteger()) { return Plain(value, 5000); }

  // Otherwise the fewest significant digits that read back to
  // the value, so one value has one text: found by halving, as
  // more digits never read back less exactly. An "f128" needs
  // at most 36.
  const auto reads_back = [&value](const unsigned digits) {
    return llvm::APFloat(value.getSemantics(), Plain(value, digits)).bitwiseIsEqual(value);
  };
  auto lo = 1u;
  auto hi = 40u;
  while (lo < hi) {
    const auto mid = (lo + hi) / 2;
    if (reads_back(mid)) { hi = mid; }
    else { lo = mid + 1; }
  }
  return Plain(value, lo);
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
