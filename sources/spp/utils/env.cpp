module;
#include <spp/macros.hpp>

module spp.utils.env;
import genex;
import std;
import sys;

namespace spp::utils::env {
  static constexpr auto kOwnPrefix = StrView("SPP_");
  static constexpr auto kCiPrefix = StrView("SPP_CI_");

  static auto ParseCount(StrView value) -> Opt<std::uint64_t>;

  /// What is wrong with a variable's value, if anything.
  static auto ProblemWith(VarSpec const &spec, StrView value) -> Opt<Str>;
}

auto spp::utils::env::ParseCount(const StrView value) -> Opt<std::uint64_t> {
  auto count = std::uint64_t{0};
  auto const *const end = value.data() + value.size();
  const auto [stop, ec] = std::from_chars(value.data(), end, count);
  if (ec != std::errc{} or stop != end) { return Nullopt; }
  return count;
}

auto spp::utils::env::ProblemWith(VarSpec const &spec, const StrView value) -> Opt<Str> {
  const auto name = Str(spec.Name);
  const auto given = ", not '" + Str(value) + "'";
  switch (spec.Kind) {
  case ValueKind::Flag:
    if (value == "0" or value == "1") { return Nullopt; }
    return name + " must be '0' or '1'" + given;
  case ValueKind::Count:
    if (ParseCount(value).has_value()) { return Nullopt; }
    return name + " must be a whole number" + given;
  case ValueKind::Choice: {
    auto listing = Str();
    for (const auto choice : spec.Choices) {
      if (value == choice) { return Nullopt; }
      listing += (listing.empty() ? "'" : ", '") + Str(choice) + "'";
    }
    return name + " must be one of " + listing + given;
  }
  case ValueKind::Text:
  default:
    return Nullopt;
  }
}

auto spp::utils::env::Schema() -> Vec<VarSpec> const& {
  static const auto schema = Vec<VarSpec>{
    VarSpec{
      .Key = Var::Path,
      .Name = "PATH",
      .Kind = ValueKind::Text,
      .Help = "Where the linker and other programs are looked for."
    },
    // The user's own directory, which is a different variable per platform;
    // only this platform's is listed.
    VarSpec{
      .Key = Var::UserDir,
#if SPP_PLATFORM_WINDOWS
      .Name = "LOCALAPPDATA",
      .Kind = ValueKind::Text,
      .Help = "Where 'spp self install' installs (%LOCALAPPDATA%\\spp\\bin)."
#else
      .Name = "HOME",
      .Kind = ValueKind::Text,
      .Help = "The user's home: 'spp self install' installs into ~/.spp/bin there."
#endif
    },
    VarSpec{
      .Key = Var::Backtrace,
      .Name = "SPP_BACKTRACE",
      .Kind = ValueKind::Flag,
      .Help = "Set to 0 to stop a built program printing a backtrace when it crashes (read by sppc)."
    },
    VarSpec{
      .Key = Var::MemoryLimitKb,
      .Name = "SPP_MEMORY_LIMIT_KB",
      .Kind = ValueKind::Count,
      .Help = "The memory cap on a program 'spp run' or 'spp test' starts, in KB (0 for none; POSIX only)."
    },
    VarSpec{
      .Key = Var::NoNetworkCheck,
      .Name = "SPP_NO_NETWORK_CHECK",
      .Kind = ValueKind::Flag,
      .Help = "Set to 1 to skip checking that dependencies' remotes are reachable."
    },
    VarSpec{
      .Key = Var::TestMode,
      .Name = "SPP_TEST_MODE",
      .Kind = ValueKind::Choice,
      .Help = "The build mode the compiler's own test suite compiles its fixtures in.",
      .Choices = {"dev", "rel"}
    },
    VarSpec{
      .Key = Var::TestOnly,
      .Name = "SPP_TEST_ONLY",
      .Kind = ValueKind::Text,
      .Help = "The tests a built test program runs; 'spp test' sets this itself."
    },
    VarSpec{
      .Key = Var::UpdateBaseUrl,
      .Name = "SPP_UPDATE_BASE_URL",
      .Kind = ValueKind::Text,
      .Help = "Where 'spp self update' looks for releases, in place of GitHub's: a URL laid out as a GitHub "
        "repository's releases (<url>/releases/latest, <url>/releases/download/<tag>/<asset>); for testing."
    },
  };
  return schema;
}

auto spp::utils::env::NameOf(
  const Var key) -> StrView {
  for (auto const &spec : Schema()) {
    if (spec.Key == key) { return spec.Name; }
  }
  std::unreachable();
}

auto spp::utils::env::Get(
  const Var key) -> Opt<Str> {
  auto const *const raw = sys::getenv(Str(NameOf(key)).c_str());
  if (raw == nullptr) { return Nullopt; }
  return Str(raw);
}

auto spp::utils::env::IsEnabled(
  const Var key) -> bool {
  return Get(key).value_or("0") == "1";
}

auto spp::utils::env::GetCount(
  const Var key) -> Opt<std::uint64_t> {
  const auto value = Get(key);
  if (not value.has_value()) { return Nullopt; }
  return ParseCount(*value);
}

auto spp::utils::env::Set(
  const Var key, const StrView value) -> void {
  sys::setenv(Str(NameOf(key)).c_str(), Str(value).c_str(), 1);
}

auto spp::utils::env::Unset(
  const Var key) -> void {
  sys::unsetenv(Str(NameOf(key)).c_str());
}

auto spp::utils::env::CheckEntries(
  Vec<StrView> const &entries, Vec<Str> &errors) -> bool {
  auto any_unknown = false;
  const auto errors_before = errors.size();

  for (const auto entry : entries) {
    // Split on the first "=". Anything outside the compiler's own prefix, and
    // anything in the prefix CI keeps for itself, is none of its business.
    const auto split = entry.find('=');
    const auto name = entry.substr(0, split);
    const auto value = split == StrView::npos ? StrView() : entry.substr(split + 1);
    if (not name.starts_with(kOwnPrefix) or name.starts_with(kCiPrefix)) { continue; }

    auto const &schema = Schema();
    const auto spec = genex::find_if(
      schema, [name](VarSpec const &s) { return s.Name == name; });
    if (spec == schema.end()) {
      errors.emplace_back("unknown environment variable '" + Str(name) + "'");
      any_unknown = true;
    }
    else if (auto problem = ProblemWith(*spec, value); problem.has_value()) {
      errors.emplace_back(std::move(*problem));
    }
  }

  if (any_unknown) {
    auto listing = Str("the variables are");
    for (auto const &spec : Schema()) {
      if (spec.Name.starts_with(kOwnPrefix)) { listing += " " + Str(spec.Name); }
    }
    errors.emplace_back(std::move(listing) + " (and '" + Str(kCiPrefix) + "*', kept for CI scripts)");
  }
  return errors.size() == errors_before;
}

auto spp::utils::env::Check(
  Vec<Str> &errors) -> bool {
  auto entries = Vec<StrView>();
  if (auto const *const *block = sys::environ(); block != nullptr) {
    for (; *block != nullptr; ++block) { entries.emplace_back(*block); }
  }
  return CheckEntries(entries, errors);
}

auto spp::utils::env::HelpText() -> Str {
  auto out = Str("\nEnvironment variables:\n");
  for (auto const &spec : Schema()) {
    if (spec.Name.starts_with(kOwnPrefix)) {
      out += "  " + Str(spec.Name) + ": " + Str(spec.Help) + "\n";
    }
  }
  return out;
}
