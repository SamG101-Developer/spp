#pragma once

#define raises_from_vec(v) \
  WithFormatters(v | genex::views::transform([](auto *scope) { return scope->GetErrorFormatter(); }) | genex::to<Vec>()) \
  .Raise()

#define ERR_ARGS(...) \
  [&]() { return spp::MakeErrArgs(__VA_ARGS__); }

#define WRAP_ERROR(c, s)                                                    \
  try {                                                                     \
    c;                                                                      \
  }                                                                         \
  catch (const SemanticError &e) {                                          \
    auto err_msg = e.what();                                                \
    Raise<SppGeneratedCodeError>({s}, ERR_ARGS(*this, std::move(err_msg))); \
  }

#define LIMIT_S(bits)                    \
  spp::MakePair(                         \
    -(boost::BigInt(1) << ((bits) - 1)), \
    (boost::BigInt(1) << ((bits) - 1)) - 1)

#define LIMIT_U(bits) \
  spp::MakePair(      \
    boost::BigInt(0), \
    (boost::BigInt(1) << (bits)) - 1)

// The largest finite value of an IEEE binary format, from the two
// parameters that define it. Taken directly rather than through
// std::numeric_limits, which is unspecialised for _Float16 and
// __float128 and silently yields a magnitude of zero there.
#define LIMIT_F_MAG(digits, max_exp) \
  ((((boost::BigInt(1) << (digits)) - 1) << ((max_exp) - (digits))).str())

#define LIMIT_F(digits, max_exp)                                   \
  spp::MakePair(                                                   \
    boost::BigDec(("-" + LIMIT_F_MAG(digits, max_exp)).c_str()),   \
    boost::BigDec(LIMIT_F_MAG(digits, max_exp).c_str()))
