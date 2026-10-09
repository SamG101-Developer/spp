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

#define IMPORT_UTILS                    \
  using namespace spp::analyse::errors; \
  using namespace spp::analyse::utils   \

#define IMPORT_UTILS_AND_UID \
  IMPORT_UTILS;              \
  using spp::utils::Uid
