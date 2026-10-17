#pragma once

#define ERR_ARGS(...) \
  [&]() { return spp::MakeErrArgs(__VA_ARGS__); }

#define WRAP_ERROR(c)                                                  \
  try {                                                                \
    c;                                                                 \
  }                                                                    \
  catch (const SemanticError &e) {                                     \
    auto err_msg = e.what();                                           \
    Raise<SppGeneratedCodeError>(ERR_ARGS(*this, std::move(err_msg))); \
  }

#define IMPORT_UTILS                    \
  using namespace spp::analyse::errors; \
  using namespace spp::analyse::utils   \

#define IMPORT_UTILS_AND_UID \
  IMPORT_UTILS;              \
  using spp::utils::Uid
