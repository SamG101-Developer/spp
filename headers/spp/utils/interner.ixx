module;
#include <spp/macros.hpp>

export module spp.utils.interner;
import spp.utils.types;
import std;

namespace spp::utils {
  /**
   * An identifier's name, reduced to an integer. Two names intern to the same id exactly when they are the same string,
   * so an id compares in one instruction and hashes in one multiply, where the string it stands for costs a wyhash pass
   * and a possible heap allocation. Names are looked up far more often than they are created - every scope on the way
   * up a lookup chain re-keys on the same name - so the string is paid for once, at the point the name is built.
   */
  SPP_EXP_CLS enum class InternedId : std::uint32_t {
    /**
     * The id of the empty string, which is what a default-constructed id is. No identifier is spelt with the empty
     * string, so this doubles as "no name" without needing a separate sentinel.
     */
    Empty = 0,
  };

  /**
   * Reduce a name to its id, assigning a new one if this is the first time the name has been seen. The string is copied
   * into storage the interner owns, so the view passed in does not have to outlive the call.
   */
  SPP_EXP_FUN SPP_ATTR_HOT
  auto Intern(StrView name) -> InternedId;
}
