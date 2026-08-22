module spp.utils.interner;
import spp.utils.types;
import std;

namespace {
  /**
   * The interner's storage. A deque, because the views handed out by "InternedView" and the keys held in the lookup map
   * both point into these strings, and a deque never moves an element it already holds. Names are never released: the
   * asts that hold ids outlive any individual compilation, and a compiler process interns a bounded set of names.
   */
  struct InternerState {
    std::deque<spp::Str> Storage;
    spp::Map<spp::StrView, spp::utils::InternedId> Ids;
    spp::Vec<spp::StrView> Views;

    InternerState() {
      // Seed the empty string, so that "InternedId::Empty" names it and a default-constructed id is valid to look up.
      Storage.emplace_back();
      Views.EmplaceBack(Storage.back());
      Ids.emplace(spp::StrView(Storage.back()), spp::utils::InternedId::Empty);
    }
  };

  auto State() -> InternerState& {
    static auto state = InternerState();
    return state;
  }
}

auto spp::utils::Intern(
  const StrView name)
  -> InternedId {
  auto &state = State();

  // Already seen, so the id it was given the first time stands.
  if (const auto it = state.Ids.find(name); it != state.Ids.end()) {
    return it->second;
  }

  // First sighting: copy the name into storage the interner owns,
  // and key the map on a view of that copy rather than of the
  // caller's string, which is not required to outlive this call.
  const auto id = static_cast<InternedId>(static_cast<std::uint32_t>(state.Views.Len()));
  auto const &owned = state.Storage.emplace_back(name);
  state.Views.EmplaceBack(owned);
  state.Ids.emplace(StrView(owned), id);
  return id;
}

auto spp::utils::InternedView(
  const InternedId id)
  -> StrView {
  return State().Views[static_cast<std::underlying_type_t<InternedId>>(id)];
}

auto spp::utils::InternedCount()
  -> std::size_t {
  return State().Views.Len();
}
