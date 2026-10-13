module;
#include <spp/macros-platforms.hpp>

#if SPP_COMPILER_MSVC
// MSVC 14.51 loses the explicit template argument of the
// "consteval" numeric_max in external_range_length's enable_if
// (C2672).
#define GCH_CONSTEVAL constexpr
#endif

#include <gch/small_vector.hpp>

export module gch;

export namespace gch {
  using ::gch::small_vector;
  using ::gch::small_vector_iterator;
  using ::gch::default_buffer_size_v;
  using ::gch::operator==;
  using ::gch::operator<=>;
  using ::gch::operator+;
  using ::gch::operator-;
  using ::gch::swap;
  using ::gch::erase;
  using ::gch::erase_if;
}
