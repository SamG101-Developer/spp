module;
#include <ankerl/unordered_dense.h>

/// The ankerl module is used for extremely performance hashmaps, hashsets, and
/// segmented equivalents for the stable iterators. The hash is also used for
/// all hashing purposes in s++ (maps/sets and interning).
export module ankerl;

export namespace ankerl::unordered_dense {
  using ::ankerl::unordered_dense::map;
  using ::ankerl::unordered_dense::set;
  using ::ankerl::unordered_dense::segmented_map;
  using ::ankerl::unordered_dense::segmented_set;
  using ::ankerl::unordered_dense::hash;
}
