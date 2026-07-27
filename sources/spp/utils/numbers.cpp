module spp.utils.numbers;

/*
auto spp::utils::numbers::IntegerBounds(
  const bool is_signed,
  const std::uint32_t bit_width)
  -> Pair<boost::BigInt, boost::BigInt> {
  const auto one = boost::BigInt(1);
  if (is_signed) {
    auto max = (one << (bit_width - 1)) - 1;
    return {-max - 1, max};
  }
  return {boost::BigInt(0), (one << bit_width) - 1};
}

auto spp::utils::numbers::FloatBounds(
  const std::uint32_t e_bits,
  const std::uint32_t m_bits)
  -> Pair<boost::BigDec, boost::BigDec> {
  const auto two = boost::BigDec(2);
  const auto x = boost::multiprecision::pow(two, boost::BigDec(m_bits));
  const auto mantissa = two - boost::BigDec(1) / x;
  const auto bias_exp = (boost::BigInt(1) << (e_bits - 1)) - 1;
  const auto max = mantissa * boost::multiprecision::pow(two, boost::BigDec(bias_exp));
  return {-max, max};
}
*/
