module;
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
