module;
#include <spp/macros.hpp>

export module spp.utils.functions;
import std;

namespace spp::utils::functions {
  SPP_EXP_CLS

  template <typename... Ts>
  struct Overload : Ts... {
    using Ts::operator()...;
  };

  SPP_EXP_CLS
  template <typename... Ts>
  Overload(Ts...) -> Overload<Ts...>;

  /// An entry in a set for as long as this lives: inserted on construction,
  /// erased on destruction, a throw included, but only if this inserted it (an
  /// entry already there is an outer call's). For recursion guards: one held in
  /// a "thread_local" set and erased by hand is left behind when what it
  /// guards throws, and "spp_tests" runs many compiles in one process, so a
  /// later compile's object at the same address is taken for one in progress.
  SPP_EXP_CLS
  template <typename S>
  class ScopedInsert {
    S &_Set;
    typename S::key_type _Key;
    bool _Inserted;

  public:
    ScopedInsert(S &set, typename S::key_type key) :
      _Set(set), _Key(key), _Inserted(set.insert(key).second) {
    }

    ScopedInsert(ScopedInsert const &) = delete;
    auto operator=(ScopedInsert const &) -> ScopedInsert& = delete;

    ~ScopedInsert() { if (_Inserted) { _Set.erase(_Key); } }

    /// Whether this inserted the entry: false when it was there already.
    [[nodiscard]] auto Inserted() const -> bool { return _Inserted; }
  };

  /// A variable holding "value" for as long as this lives, and its old value
  /// again on destruction, a throw included. For state a nested call replaces
  /// and its caller gets back: the instantiation being analysed, a depth
  /// ("ScopedAssign(depth, depth + 1)"), the bindings of a folded call.
  SPP_EXP_CLS
  template <typename T>
  class ScopedAssign {
    T &_Var;
    T _Outer;

  public:
    /// "T" from the variable alone: "value" converts to it ("&sub" into a
    /// "const" pointer, "depth + 1" into the depth's type).
    ScopedAssign(T &var, std::type_identity_t<T> value) :
      _Var(var), _Outer(std::exchange(var, std::move(value))) {
    }

    ScopedAssign(ScopedAssign const &) = delete;
    auto operator=(ScopedAssign const &) -> ScopedAssign& = delete;

    ~ScopedAssign() { _Var = std::move(_Outer); }
  };
}
