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
    -(numex::BigInt(1) << ((bits) - 1)), \
    (numex::BigInt(1) << ((bits) - 1)) - 1)

#define LIMIT_U(bits) \
  spp::MakePair(      \
    numex::BigInt(0), \
    (numex::BigInt(1) << (bits)) - 1)

#define LIMIT_F_MAG(digits, max_exp) \
  ((((numex::BigInt(1) << (digits)) - 1) << ((max_exp) - (digits))).ToString())

#define LIMIT_F(digits, max_exp)                                 \
  spp::MakePair(                                                 \
    numex::BigDec(("-" + LIMIT_F_MAG(digits, max_exp)).c_str()), \
    numex::BigDec(LIMIT_F_MAG(digits, max_exp).c_str()))
