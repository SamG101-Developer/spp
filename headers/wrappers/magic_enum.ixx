module;
#include <magic_enum/magic_enum.hpp>

export module magic_enum;

export namespace magic_enum {
  using ::magic_enum::case_insensitive;
  using ::magic_enum::enum_cast;
  using ::magic_enum::enum_entries;
  using ::magic_enum::enum_name;
  using ::magic_enum::enum_values;
}
