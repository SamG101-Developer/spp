module;
#include <spp/macros.hpp>

module spp.utils.features;
import spp.utils.files;
import spp.utils.strings;
import genex;
import tomlpp;

namespace {
  /**
   * The settings in force; populated from the schema's defaults and then from the config. The map is defined as as
   * "feature" : "enabled". It gets pulled by reference and updated, so the single static instance is fine to use, it
   * doesn't produce a fresh map for example.
   */
  auto& CurrentFlags() {
    static auto flags = spp::Map<spp::utils::features::ConfigKey, bool>();
    return flags;
  }

  /**
   * Whether the defaults have been put in place yet, so that a query before any load still answers correctly. Once set
   * to true, it will always be true until the program ends.
   */
  auto& FlagsReady() {
    static auto ready = false;
    return ready;
  }

  /**
   * Set all the feature flags to their default values, and mark all flags as "ready".
   */
  auto ApplyDefaults() -> void {
    auto &flags = CurrentFlags();
    for (auto const &section : spp::utils::features::Schema()) {
      for (auto const &k : section.Keys) {
        if (k.Kind == spp::utils::features::ValueKind::Bool) { flags[k.Key] = k.Default; }
      }
    }
    FlagsReady() = true;
  }

  /**
   * The section a dotted path names, or null when the schema has no such section. For example `[memory.stack]` will
   * retrieve its section, but `[a.b.c]` is invalid and therefore returns `nullptr`.
   */
  auto FindSection(
    spp::StrView name)
    -> spp::utils::features::SectionSpec const* {
    for (auto const &section : spp::utils::features::Schema()) {
      if (section.Name == name) { return &section; }
    }
    return nullptr;
  }

  /**
   * Provide a "did you mean" for a name that is not in the schema (empty when nothing is close enough to suggest). It
   * reuses the compiler's string comparison usually reserved for "unidentified symbol" within the compilation process.
   */
  auto Suggest(
    spp::StrView name,
    spp::Vec<spp::Str> const &choices)
    -> spp::Str {
    const auto closest = spp::utils::strings::ClosestMatch(name, choices);
    return closest.has_value() ? " (did you mean '" + *closest + "'?)" : spp::Str();
  }

  /**
   * String format the "kind" for error reporting.
   * @param kind The enum "value kind" that a key must contain a value of.
   * @return The stringified value.
   */
  auto KindName(
    const spp::utils::features::ValueKind kind)
    -> spp::StrView {
    switch (kind) {
      case spp::utils::features::ValueKind::Bool: return "a boolean";
      case spp::utils::features::ValueKind::String: return "a string";
      case spp::utils::features::ValueKind::Table: return "a table";
      default: return "a value";
    }
  }

  /**
   * Walk the config, reporting anything the schema does not describe.
   */
  auto WalkTable(
    auto const &table,
    spp::Str const &prefix,
    spp::Vec<spp::Str> &errors)
    -> void {
    //
    using namespace spp::utils::features;

    // Every section name, and every one that is a section or
    // a step on the way to one, so that "[memory]" on its own
    // is a container rather than a mistake.
    auto section_names = spp::Vec<spp::Str>();
    for (auto const &section : Schema()) {
      section_names.EmplaceBack(spp::Str(section.Name));
    }

    for (auto &&[raw_key, node] : table) {
      const auto key = spp::Str(raw_key.str());
      const auto path = prefix.empty() ? key : prefix + "." + key;

      if (auto const *const section = FindSection(path); section != nullptr) {
        // A section: everything under it must be one of its keys, and the keys it must have must be there.
        auto const *const inner = node.as_table();
        if (inner == nullptr) {
          errors.EmplaceBack("'[" + path + "]' must be a section, not a value");
          continue;
        }

        auto key_names = spp::Vec<spp::Str>();
        for (auto const &k : section->Keys) { key_names.EmplaceBack(spp::Str(k.Name)); }

        for (auto &&[raw_inner_key, inner_node] : *inner) {
          const auto inner_key = spp::Str(raw_inner_key.str());
          auto const *spec = static_cast<KeySpec const*>(nullptr);
          for (auto const &k : section->Keys) {
            if (k.Name == inner_key) {
              spec = &k;
              break;
            }
          }

          if (spec == nullptr) {
            if (section->FreeForm) { continue; }
            errors.EmplaceBack(
              "unknown key '" + inner_key + "' in [" + path + "]" + Suggest(inner_key, key_names)
              + "\n    [" + path + "] accepts: " + (key_names.IsEmpty()
                ? spp::Str("(nothing)")
                : [&] {
                  auto joined = spp::Str();
                  for (auto const &n : key_names) { joined += (joined.empty() ? "" : ", ") + n; }
                  return joined;
                }()));
            continue;
          }

          const auto ok =
            spec->Kind == ValueKind::Bool
            ? inner_node.is_boolean() or inner_node.is_integer()
            : spec->Kind == ValueKind::String
            ? inner_node.is_string()
            : inner_node.is_table();
          if (not ok) {
            errors.EmplaceBack("'" + inner_key + "' in [" + path + "] must be " + spp::Str(KindName(spec->Kind)));
          }
        }

        for (auto const &k : section->Keys) {
          if (k.Required and not inner->contains(k.Name)) {
            errors.EmplaceBack("[" + path + "] is missing the required key '" + spp::Str(k.Name) + "'");
          }
        }
        continue;
      }

      // Not a section itself. It is fine only as a step towards one, and only if it is a table to step into.
      const auto is_prefix = genex::any_of(section_names, [&](spp::Str const &n) {
        return n.size() > path.size() and n.starts_with(path) and n[path.size()] == '.';
      });
      if (is_prefix and node.is_table()) {
        WalkTable(*node.as_table(), path, errors);
        continue;
      }

      // Report the path as it was written. Toml nests, so "[memry.stack]" arrives as an unknown "memry" holding a
      // "stack", and naming only the outer half describes something the author never typed.
      auto written = path; // as spelled in the file, not just the outermost table
      for (auto const *walk = node.as_table(); walk != nullptr and walk->size() == 1;) {
        auto const *descend = static_cast<toml::table const*>(nullptr);
        for (auto &&[only_key, only_node] : *walk) {
          if (not only_node.is_table()) { break; }
          written += "." + spp::Str(only_key.str());
          descend = only_node.as_table();
        }
        walk = descend;
      }

      errors.EmplaceBack(
        "unknown section '[" + written + "]'" + Suggest(written, section_names)
        + "\n    known sections: " + [&] {
          auto joined = spp::Str();
          for (auto const &n : section_names) { joined += (joined.empty() ? "" : ", ") + n; }
          return joined;
        }());
    }
  }
}

auto spp::utils::features::Schema()
  -> Vec<SectionSpec> const& {
  // Written out rather than registered from the places that
  // read it, so that the whole surface of a project file is
  // one thing to read, and so a feature cannot exist without
  // being documented here.
  static const auto schema = Vec<SectionSpec>{
    SectionSpec{
      .Name = "project",
      .Help = "What the project is.",
      .Keys = Vec<KeySpec>{
        KeySpec{
          .Key = ConfigKey::ProjectName,
          .Name = "name",
          .Kind = ValueKind::String,
          .Required = true,
          .Default = false,
          .Help = "The project's name."
        },
        KeySpec{
          .Key = ConfigKey::ProjectVersion,
          .Name = "version",
          .Kind = ValueKind::String,
          .Required = true,
          .Default = false,
          .Help = "Its version, as 'major.minor.patch'."
        },
        KeySpec{
          .Key = ConfigKey::ProjectBuild,
          .Name = "build",
          .Kind = ValueKind::String,
          .Required = true,
          .Default = false,
          .Help = "What to produce: 'exe' or 'lib'."
        },
      },
      .FreeForm = false,
    },
    SectionSpec{
      .Name = "vcs",
      .Help = "Dependencies, one key per library, each a table of 'git' and an optional 'branch'.",
      .Keys = Vec<KeySpec>{},
      .FreeForm = true,
    },
    SectionSpec{
      .Name = "memory.stack",
      .Help = "Protections applied to a function's own frame.",
      .Keys = Vec<KeySpec>{
        KeySpec{
          .Key = ConfigKey::MemoryStackProtect,
          .Name = "protect",
          .Kind = ValueKind::Bool,
          .Required = false,
          .Default = true,
          .Help = "Put a canary between a frame's locals and its return address, and check it before the frame is left."
        },
      },
      .FreeForm = false,
    },
  };
  return schema;
}

auto spp::utils::features::Validate(
  std::filesystem::path const &config,
  Vec<Str> &errors)
  -> bool {
  // Check the spp toml is present. Failsafe but needed
  // so we don't get worse errors downstream.
  if (not std::filesystem::exists(config)) {
    errors.EmplaceBack("no 'spp.toml' here");
    return false;
  }

  // Parse the file and walk the table, otherwise if the
  // file is bad, report an error. This will be an error
  // with the toml structure / syntax, not the key/value
  // validity based off the schema. WalkTable will throw
  // the semantic errors.
  try {
    const auto parsed = toml::parse_file(utils::files::NativeString(config));
    toml::table const &root = parsed;
    WalkTable(root, Str(), errors);
  }
  catch (std::exception const &e) {
    errors.EmplaceBack(Str("could not read 'spp.toml': ") + e.what());
  }

  // Validity of semantics is enforced by the tracking
  // list remaining empty.
  return errors.IsEmpty();
}

auto spp::utils::features::Load(
  std::filesystem::path const &config,
  Vec<Str> &errors)
  -> bool {
  Reset();
  if (not Validate(config, errors)) { return false; }

  // Prase the toml file and get the current feature set
  // for modification.
  const auto parsed = toml::parse_file(utils::files::NativeString(config));
  toml::table const &root = parsed;
  auto &flags = CurrentFlags();

  // Iterate through the schema; there will be no key/value
  // pairs in the validated toml (or section names), that
  // don't exist in the schema.
  for (auto const &section : Schema()) {

    // Move through the keys of the section.
    for (auto const &k : section.Keys) {
      if (k.Kind != ValueKind::Bool) { continue; }

      // The section's name is dotted, so it is walked a
      // step at a time rather than looked up whole.
      auto const *node = static_cast<toml::node const*>(&root);
      auto rest = StrView(section.Name);
      while (node != nullptr and not rest.empty()) {
        const auto dot = rest.find('.');
        const auto part = Str(rest.substr(0, dot));
        rest = dot == StrView::npos ? StrView() : rest.substr(dot + 1);
        auto const *const as_table = node->as_table();
        node = as_table != nullptr and as_table->contains(part) ? as_table->get(part) : nullptr;
      }

      // A nullptr node means that the section from the
      // schema simply wasn't present in the toml (not all
      // are required), so continue to the next key/section.
      if (node == nullptr) { continue; }

      // Check that the key is present in the table. If it
      // isn't, then continue, as this key wasn't required.
      auto const *const as_table = node->as_table();
      if (as_table == nullptr or not as_table->contains(k.Name)) {
        continue;
      }

      // Enforce "false" and "true" are used, not "0" and "1",
      // because we are aiming for single-styled, uniform config
      // files. Semantically there's no difference of course,
      // just enforced styling.
      auto const *const value = as_table->get(k.Name);
      if (const auto b = value->value<bool>()) { flags[k.Key] = *b; }
      else if (const auto i = value->value<std::int64_t>()) { flags[k.Key] = *i != 0; }
    }
  }

  // Everything loaded.
  return true;
}

auto spp::utils::features::Enabled(
  const ConfigKey key)
  -> bool {
  // If the flags aren't ready, apply the default values
  // before-hand, as a failsafe. This also fills non-required
  // flag's values with their default.
  if (not FlagsReady()) { ApplyDefaults(); }

  // Get the flags's value, and ensure the flag was actually
  // found in the feature set too.
  const auto it = CurrentFlags().find(key);
  SPP_ASSERT(it != CurrentFlags().end());
  return it->second;
}

auto spp::utils::features::Reset()
  -> void {
  // Clear all the flags' values, and set the default in.
  CurrentFlags().clear();
  ApplyDefaults();
}

auto spp::utils::features::HelpText()
  -> Str {
  auto out = Str("Sections and keys accepted in 'spp.toml':\n");
  for (auto const &section : Schema()) {
    out += "\n  [" + Str(section.Name) + "]\n    " + Str(section.Help) + "\n";
    if (section.FreeForm) {
      out += "    (any key)\n";
      continue;
    }
    for (auto const &k : section.Keys) {
      out += "    " + Str(k.Name);
      out += k.Kind == ValueKind::Bool
        ? Str(" = ") + (k.Default ? "true" : "false") + "  (default)"
        : k.Required
        ? Str("  (required)")
        : Str();
      out += "\n      " + Str(k.Help) + "\n";
    }
  }
  return out;
}
