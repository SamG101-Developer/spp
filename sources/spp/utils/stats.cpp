module;
#include <spp/macros.hpp>
#include <spp/version.hpp>

module spp.utils.stats;
import spp.utils.types;
import std;
import sys;

namespace {
  using Clock = std::chrono::steady_clock;

  /// What each counter is called in the report, by "Counter".
  constexpr auto kCounterNames = spp::Arr<
    spp::StrView, spp::utils::stats::kCounterCount>{
    "ast_nodes_created",
    "scopes_created",
    "scopes_copied",
    "type_symbol_lookups",
    "type_symbol_lookups_by_stamp",
    "var_symbol_lookups",
    "sup_scope_walks",
    "sup_scope_walks_cached",
    "type_key_interns",
    "comp_key_interns",
    "type_keys_distinct",
    "comp_keys_distinct",
    "scope_linkage_generation",
    "type_structure_generation",
    "cls_instances_made",
    "fn_instances_made",
    "sup_instances_made",
  };

  using CounterArr = spp::Arr<std::uint64_t, spp::utils::stats::kCounterCount>;

  /// One closed phase: how long it took, the high-water mark of memory at its
  /// end, and how much of each counter it used. Effectively a span of "begin
  /// here ... keep recording ... end here".
  struct Phase {
    spp::Str Name;
    std::uint64_t WallNs;
    std::uint64_t PeakRssBytes;
    CounterArr Deltas;
  };

  /// The master state of all statistic management.
  struct State {
    bool Enabled = false;
    spp::Path File;
    spp::Func<void()> Sampler;
    spp::Vec<Phase> Phases;
    spp::Vec<spp::Tup<spp::Str, std::uint64_t>> Records;

    spp::Opt<spp::Str> OpenName; /// The open phase, if "OpenName" has a value.
    Clock::time_point OpenAt;
    CounterArr OpenCounts{};
    Clock::time_point StartedAt;
  };

  /// Get the state as a static singleton borrow, for all state modification and
  /// reading.
  auto TheState() -> State& {
    static auto state = State();
    return state;
  }

  ///
  auto Sample() -> CounterArr {
    auto &state = TheState();
    if (state.Sampler) { state.Sampler(); }
    return spp::utils::stats::Counts;
  }

  auto ClosePhase() -> void {
    auto &state = TheState();
    if (not state.OpenName.has_value()) { return; }

    const auto now = Clock::now();
    const auto counts = Sample();
    auto phase = Phase{
      .Name = std::move(*state.OpenName),
      .WallNs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
          now - state.OpenAt).count()),
      .PeakRssBytes = sys::peak_rss_bytes(),
      .Deltas = {}
    };
    for (auto i = 0uz; i < counts.size(); ++i) {
      phase.Deltas[i] = counts[i] - state.OpenCounts[i];
    }
    state.Phases.emplace_back(std::move(phase));
    state.OpenName.reset();
  }

  auto JsonCounters(CounterArr const &counts) -> spp::Str {
    auto out = spp::Str("{");
    for (auto i = 0uz; i < counts.size(); ++i) {
      out += std::format(
        "{}\"{}\":{}", i > 0 ? "," : "", kCounterNames[i], counts[i]);
    }
    return out + "}";
  }

  /// The mode and target are names the compiler chose, but a target triple is
  /// still escaped rather than trusted.
  auto JsonString(const spp::StrView text) -> spp::Str {
    auto out = spp::Str("\"");
    for (const auto c : text) {
      if (c == '"' or c == '\\') { out += '\\'; }
      out += c;
    }
    return out + "\"";
  }
}

spp::Arr<std::uint64_t, spp::utils::stats::kCounterCount> spp::utils::stats::Counts{};

auto spp::utils::stats::Enable(Path file) -> void {
  auto &state = TheState();
  state = State();
  state.Enabled = true;
  state.File = std::move(file);
  state.StartedAt = Clock::now();
}

auto spp::utils::stats::Enabled() -> bool {
  return TheState().Enabled;
}

auto spp::utils::stats::SetSampler(Func<void()> sampler) -> void {
  TheState().Sampler = std::move(sampler);
}

auto spp::utils::stats::BeginPhase(const StrView name) -> void {
  // The counters run whether or not anything records them; only the phases
  // cost anything, so they are skipped unless asked for.
  auto &state = TheState();
  if (not state.Enabled) { return; }
  ClosePhase();
  state.OpenName = Str(name);
  state.OpenCounts = Sample();
  state.OpenAt = Clock::now();
}

auto spp::utils::stats::Record(
  const StrView name, const std::uint64_t value) -> void {
  auto &state = TheState();
  if (not state.Enabled) { return; }
  state.Records.emplace_back(Str(name), value);
}

auto spp::utils::stats::Finish(
  const StrView mode, const StrView target, const bool built) -> bool {
  auto &state = TheState();
  if (not state.Enabled) { return true; }
  ClosePhase();

  // The totals are the sums of the phases, so the counts before the first one
  // (process start-up, reading spp.toml) are not in either.
  auto totals = CounterArr{};
  for (auto const &phase : state.Phases) {
    for (auto i = 0uz; i < totals.size(); ++i) { totals[i] += phase.Deltas[i]; }
  }
  const auto wall_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
    Clock::now() - state.StartedAt).count();

  auto out = Str("{\"schema\":1");
  out += std::format(",\"spp\":{}", JsonString(SPP_VERSION));
  out += ",\"mode\":" + JsonString(mode);
  out += ",\"target\":" + JsonString(target);
  out += std::format(",\"built\":{}", built ? "true" : "false");
  out += std::format(
    ",\"total\":{{\"wall_ns\":{},\"peak_rss_bytes\":{},\"counters\":{}}}",
    wall_ns, sys::peak_rss_bytes(), JsonCounters(totals));

  out += ",\"phases\":[";
  for (auto i = 0uz; i < state.Phases.size(); ++i) {
    auto const &phase = state.Phases[i];
    out += std::format(
      "{}{{\"name\":{},\"wall_ns\":{},\"peak_rss_bytes\":{},\"counters\":{}}}",
      i > 0 ? "," : "", JsonString(phase.Name), phase.WallNs,
      phase.PeakRssBytes, JsonCounters(phase.Deltas));
  }
  out += "]";

  out += ",\"records\":{";
  for (auto i = 0uz; i < state.Records.size(); ++i) {
    auto const &[name, value] = state.Records[i];
    out += std::format("{}{}:{}", i > 0 ? "," : "", JsonString(name), value);
  }
  out += "}}\n";

  auto file = std::ofstream(state.File, std::ios::binary | std::ios::trunc);
  file << out;
  file.close();
  state.Enabled = false;
  if (not file) {
    std::cerr
      << "Error: Could not write the stats to '"
      << state.File.string() << "'.\n";
    return false;
  }
  return true;
}
