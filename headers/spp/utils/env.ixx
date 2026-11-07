module;
#include <spp/macros.hpp>

export module spp.utils.env;
import spp.utils.types;
import std;

namespace spp::utils::env {
  /// Every environment variable the compiler, the programs it builds and its
  /// tests read. Names under "SPP_" that aren't here are rejected at startup,
  /// except "SPP_CI_", which belongs to the CI scripts.
  SPP_EXP_CLS enum class Var {
    Path,
    Backtrace,
    MemoryLimitKb,
    NoNetworkCheck,
    TestMode,
    TestOnly,
  };

  /// What a variable's value may be: anything, "0" or "1", a whole number, or
  /// one of a fixed list.
  SPP_EXP_CLS enum class ValueKind { Text, Flag, Count, Choice };

  /// A variable's name, what its value may be, and its help text.
  SPP_EXP_CLS struct VarSpec {
    Var Key;
    StrView Name;
    ValueKind Kind;
    StrView Help;
    Vec<StrView> Choices = {}; /// The accepted values of a "Choice".
  };

  /// Every variable, which is what an unrecognised one is reported against.
  SPP_EXP_FUN auto Schema() -> Vec<VarSpec> const&;

  /// The name a variable has in the environment.
  SPP_EXP_FUN auto NameOf(Var key) -> StrView;

  /// The value of a variable, or none when it is not set.
  SPP_EXP_FUN auto Get(Var key) -> Opt<Str>;

  /// Whether a "Flag" variable is set to "1".
  SPP_EXP_FUN auto IsEnabled(Var key) -> bool;

  /// The value of a "Count" variable, or none when it is not set or is not a
  /// whole number.
  SPP_EXP_FUN auto GetCount(Var key) -> Opt<std::uint64_t>;

  /// Set a variable, replacing any value it had. On Windows an empty value
  /// unsets it.
  SPP_EXP_FUN auto Set(Var key, StrView value) -> void;

  /// Remove a variable, if it is set.
  SPP_EXP_FUN auto Unset(Var key) -> void;

  /// Check "NAME=value" entries, reporting each unknown "SPP_" name and each
  /// value its variable does not accept.
  SPP_EXP_FUN auto CheckEntries(
    Vec<StrView> const &entries, Vec<Str> &errors) -> bool;

  /// Run "CheckEntries" over this process's environment.
  SPP_EXP_FUN auto Check(Vec<Str> &errors) -> bool;

  /// Every "SPP_" variable as a listing, for the command "spp config".
  SPP_EXP_FUN auto HelpText() -> Str;
}
