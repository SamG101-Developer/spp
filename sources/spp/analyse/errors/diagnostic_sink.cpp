module spp.analyse.errors.diagnostic_sink;
import spp.analyse.errors.semantic_error;
import spp.utils.types;
import std;

namespace spp::analyse::errors::diagnostic_sink {
  namespace {
    /// [CHECKED]
    /// The state is the struct that holds all the diagnostic
    /// info for the build, that accumulates and will be used
    /// for IntelliJ error reporting,
    struct State {
      bool Enabled = false;
      Vec<SemanticError> Errors;
      Set<Ast const*> Poisoned;
    };

    /// [CHECKED]
    /// The singleton instance of the State, used in all the
    /// diagnostic methods. Shared on all calls.
    auto Current() -> State& {
      static auto state = State();
      return state;
    }
  }
}

auto spp::analyse::errors::diagnostic_sink::Enable(
  const bool enabled) -> void {
  // Mark the static sink as enabled.
  Current().Enabled = enabled;
}

auto spp::analyse::errors::diagnostic_sink::IsEnabled() -> bool {
  // Check if the state has been enabled for usage.
  return Current().Enabled;
}

auto spp::analyse::errors::diagnostic_sink::Report(
  SemanticError const &error, Ast const *const member) -> void {
  // Add the error into the list, and poison the member.
  Current().Errors.EmplaceBack(error);
  if (member != nullptr) { Current().Poisoned.insert(member); }
}

auto spp::analyse::errors::diagnostic_sink::IsPoisoned(
  Ast const *const member) -> bool {
  // Check if this member has been marked as poisoned.
  return member != nullptr and Current().Poisoned.contains(member);
}

auto spp::analyse::errors::diagnostic_sink::Collected()
  -> Vec<SemanticError> const& {
  // Getter for the errors, for json for the IDEA plugin.
  return Current().Errors;
}

auto spp::analyse::errors::diagnostic_sink::HasErrors()
  -> bool {
  // If the errors list is not empty, 1 or more have been
  // raised.
  return not Current().Errors.IsEmpty();
}

auto spp::analyse::errors::diagnostic_sink::Clear() -> void {
  Current().Errors.Clear();
  Current().Poisoned.clear();
}
