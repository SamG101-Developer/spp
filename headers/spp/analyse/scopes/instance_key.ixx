module;
#include <spp/macros.hpp>

export module spp.analyse.scopes.instance_key;
import spp.utils.interner;
import spp.utils.types;
import std;

namespace spp::analyse::scopes {
  /// The identity an instantiation is filed under: what each
  /// argument resolves to, as tagged words - the tag in the top
  /// byte and a payload below it, or a pointer in a word of its
  /// own after a "Sym" word. Every tag is followed by a fixed
  /// shape, so two keys are equal exactly when their arguments
  /// are. A key is only compared and hashed, never printed.
  SPP_EXP_CLS struct InstanceKey {
    enum class Tag : std::uint8_t {
      Name, Pos, Conv, Unresolved, Bound, Param, Self, Sym, Comp
    };

    std::vector<std::uint64_t> Words;
    std::uint64_t Hash = 0;

    /// Append a tag with a small payload (an id, a position, a
    /// convention).
    auto Push(const Tag tag, const std::uint64_t payload = 0) -> void {
      Append(static_cast<std::uint64_t>(tag) << 56 | payload);
    }

    /// Append a tag with text that is keyed by its spelling,
    /// interned.
    auto PushText(const Tag tag, const StrView text) -> void {
      Push(tag, static_cast<std::uint64_t>(utils::Intern(text)));
    }

    /// Append a symbol's identity: its address.
    auto PushPtr(void const *const ptr) -> void {
      Push(Tag::Sym);
      Append(reinterpret_cast<std::uintptr_t>(ptr));
    }

    auto operator==(InstanceKey const &that) const -> bool {
      return Hash == that.Hash and Words == that.Words;
    }

  private:
    auto Append(const std::uint64_t word) -> void {
      Words.push_back(word);
      Hash = (Hash ^ word) * 0x9E3779B97F4A7C15ull;
      Hash ^= Hash >> 29;
    }
  };

  /// A key hashes as the hash it accumulated while it was built.
  SPP_EXP_CLS struct InstanceKeyHash {
    auto operator()(InstanceKey const &key) const noexcept -> std::uint64_t { return key.Hash; }
  };
}
