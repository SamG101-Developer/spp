#include "test_macros.hpp"

import spp.utils.numbers;
import llvm;

// A float's text is part of its comp identity ("ValueKeyOf"): one value has one text, the fewest significant digits
// that read back to it.

namespace {
  auto TextOf(const spp::StrView literal, spp::Str const &type) -> spp::Str {
    return spp::utils::numbers::WriteFloat(spp::utils::numbers::ReadFloat(literal, type)).value();
  }
}

TEST(TestFloatText, test_fewest_digits_that_read_back) {
  EXPECT_EQ(TextOf("0.1", "f64"), "0.1");
  EXPECT_EQ(TextOf("0.1", "f32"), "0.1");
  EXPECT_EQ(TextOf("0.30000000000000004", "f64"), "0.30000000000000004");
  EXPECT_EQ(TextOf("1.5", "f16"), "1.5");
  EXPECT_EQ(TextOf("-2.5", "f64"), "-2.5");
}

TEST(TestFloatText, test_spellings_of_one_value_share_a_text) {
  EXPECT_EQ(TextOf("1.50", "f64"), "1.5");
  EXPECT_EQ(TextOf("0.1000000000000000055", "f64"), "0.1");
}

TEST(TestFloatText, test_rounding_carries_through_digits) {
  EXPECT_EQ(TextOf("0.999999", "f32"), "0.999999");
  EXPECT_EQ(TextOf("9.999999", "f32"), "9.999999");
  EXPECT_EQ(TextOf("99.5", "f32"), "99.5");

  // Rounded from the exact digits: "toString" truncates first, and wrote these as "0.00000739" and "81745.265".
  EXPECT_EQ(TextOf("0.0000074", "f16"), "0.0000074");
  EXPECT_EQ(TextOf("81745.265625", "f32"), "81745.266");
}

TEST(TestFloatText, test_smallest_values_are_written_plainly) {
  EXPECT_EQ(TextOf("0.00000000000000000000000000000000000001175494350822287507968736537222245677819", "f32"),
    "0.000000000000000000000000000000000000011754944");
  EXPECT_EQ(TextOf("0.00000011920928955078125", "f32"), "0.00000011920929");
}

TEST(TestFloatText, test_integers_are_written_exactly) {
  EXPECT_EQ(TextOf("0.0", "f64"), "0");
  EXPECT_EQ(TextOf("100.0", "f64"), "100");
  EXPECT_EQ(TextOf("340282346638528859811704183484516925440.0", "f32"), "340282346638528859811704183484516925440");
}

TEST(TestFloatText, test_infinity_has_no_text) {
  const auto value = llvm::APFloat::getInf(spp::utils::numbers::ReadFloat("1.0", "f64").getSemantics());
  EXPECT_FALSE(spp::utils::numbers::WriteFloat(value).has_value());
}
