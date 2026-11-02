module;
#include <spp/macros.hpp>

export module spp.utils.stats;
import spp.utils.types;
import std;

namespace spp::utils::stats {
  /// The work a compile does, counted where it is done. Counts are
  /// machine-independent: the same compiler on the same input gives the same
  /// numbers on any machine, which is what lets "spp build --emit-stats" gate
  /// on them exactly.
  SPP_EXP_CLS enum class Counter : std::uint8_t {
    AstNodesCreated,
    ScopesCreated,
    ScopesCopied,
    TypeSymbolLookups,
    TypeSymbolLookupsByStamp,
    VarSymbolLookups,
    SupScopeWalks,
    SupScopeWalksCached,
    TypeKeyInterns,
    CompKeyInterns,
    TypeKeysDistinct,
    CompKeysDistinct,
    ScopeLinkageGeneration,
    TypeStructureGeneration,
    ClsInstancesMade,
    FnInstancesMade,
    SupInstancesMade,
    Count_,
  };

  SPP_EXP_CMP constexpr auto kCounterCount = static_cast<std::size_t>(
    Counter::Count_);

  /// The running totals, by "Counter". Zero-initialised, so constant
  /// initialised, which MSVC needs of an exported inline variable.
  SPP_EXP_CMP auto Counts = Arr<std::uint64_t, kCounterCount>{};

  /// Count one unit of work. Always on: one increment is cheaper than the
  /// check that would skip it.
  SPP_EXP_FUN SPP_ATTR_HOT inline auto Bump(const Counter counter) -> void {
    ++Counts[static_cast<std::size_t>(counter)];
  }

  /// Set a counter that mirrors a value kept elsewhere (an interned set's size,
  /// a generation). Only called from a sampler, at a phase's end.
  SPP_EXP_FUN inline auto Set(
    const Counter counter, const std::uint64_t value) -> void {
    Counts[static_cast<std::size_t>(counter)] = value;
  }

  /// Start recording, to write the report to "file" at "Finish". Before this,
  /// phases and values are not recorded.
  SPP_EXP_FUN auto Enable(Path file) -> void;

  /// Getter for checking if the phases are being recorded or not.
  SPP_EXP_FUN auto Enabled() -> bool;

  /// Called at every phase's end, before its counts are read, to "Set" the
  /// counters this module cannot see.
  SPP_EXP_FUN auto SetSampler(Func<void()> sampler) -> void;

  /// Close the current phase, if any, and start "name". A phase's numbers are
  /// what changed between its start and the next phase's.
  SPP_EXP_FUN auto BeginPhase(StrView name) -> void;

  /// A named measurement that is not a running count, such as an IR size.
  SPP_EXP_FUN auto Record(StrView name, std::uint64_t value) -> void;

  /// Close the last phase and write the report. False if it could not be
  /// written.
  SPP_EXP_FUN auto Finish(StrView mode, StrView target, bool built) -> bool;
}
