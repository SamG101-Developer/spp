module;
#include <spp/macros.hpp>

module spp.utils.features;
import spp.utils.files;
import spp.utils.ice;
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
   * The values of string and version keys, defaulted then set, as for the flags above.
   */
  auto& CurrentTexts() {
    static auto texts = spp::Map<spp::utils::features::ConfigKey, spp::Str>();
    return texts;
  }

  /**
   * The linkers "[target.linkers]" names, by target folder.
   */
  auto& CurrentLinkers() {
    static auto linkers = spp::Map<spp::Str, spp::Str>();
    return linkers;
  }

  /**
   * Every key the loaded config sets itself, whatever its value.
   */
  auto& CurrentSet() {
    static auto set = spp::Set<spp::utils::features::ConfigKey>();
    return set;
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
    auto &texts = CurrentTexts();
    for (auto const &section : spp::utils::features::Schema()) {
      for (auto const &k : section.Keys) {
        if (k.Kind == spp::utils::features::ValueKind::Bool) { flags[k.Key] = k.Default; }
        else if (not k.DefaultText.empty()) { texts[k.Key] = spp::Str(k.DefaultText); }
      }
    }
    FlagsReady() = true;
  }

  /**
   * The section and spec of a key; every key is in the schema exactly once.
   */
  auto FindKey(
    const spp::utils::features::ConfigKey key)
    -> spp::Pair<spp::utils::features::SectionSpec const*, spp::utils::features::KeySpec const*> {
    for (auto const &section : spp::utils::features::Schema()) {
      for (auto const &k : section.Keys) {
        if (k.Key == key) { return {&section, &k}; }
      }
    }
    spp::Ice("config key missing from the schema");
  }

  /**
   * Whether a string is a dotted version, "major.minor" or longer, every part digits.
   */
  auto IsVersion(
    const spp::StrView text)
    -> bool {
    auto parts = 0uz;
    auto digits = 0uz;
    for (const auto c : text) {
      if (c == '.') {
        if (digits == 0) { return false; }
        parts += 1;
        digits = 0;
      }
      else if (c >= '0' and c <= '9') { digits += 1; }
      else { return false; }
    }
    return digits > 0 and parts >= 1;
  }

  /**
   * Whether version "a" is older than version "b", compared part by part, a missing part being 0.
   */
  auto VersionLess(
    const spp::StrView a,
    const spp::StrView b)
    -> bool {
    const auto parts = [](const spp::StrView v) {
      auto out = spp::Vec<std::uint64_t>{0};
      for (const auto c : v) {
        if (c == '.') { out.emplace_back(0); }
        else { out.back() = out.back() * 10 + static_cast<std::uint64_t>(c - '0'); }
      }
      return out;
    };
    auto pa = parts(a);
    auto pb = parts(b);
    const auto size = std::max(pa.size(), pb.size());
    pa.resize(size, 0);
    pb.resize(size, 0);
    return pa < pb;
  }

  /**
   * The section a dotted path names, or null when the schema has no such section. For example `[memory.stack]` will
   * retrieve its section, but `[a.b.c]` is invalid and therefore returns `nullptr`.
   */
  auto FindSection(
    const spp::StrView name)
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
    const spp::StrView name,
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
      case spp::utils::features::ValueKind::Version: return "a version like '4.18'";
      case spp::utils::features::ValueKind::Table: return "a table";
      default: spp::Ice("unknown config value kind");
    }
  }

  /**
   * Walk the config, reporting anything the schema does not describe.
   */
  auto WalkTable(
    auto const &table, spp::Str const &prefix,
    spp::Vec<spp::Str> &errors) -> void {
    //
    using namespace spp::utils::features;

    // Every section name, and every one that is a section or
    // a step on the way to one, so that "[memory]" on its own
    // is a container rather than a mistake.
    auto section_names = spp::Vec<spp::Str>();
    for (auto const &section : Schema()) {
      section_names.emplace_back(spp::Str(section.Name));
    }

    for (auto &&[raw_key, node] : table) {
      const auto key = spp::Str(raw_key.str());
      const auto path = prefix.empty() ? key : prefix + "." + key;

      if (auto const *const section = FindSection(path); section != nullptr) {
        // A section: everything under it must be one of its keys, and the keys it must have must be there.
        auto const *const inner = node.as_table();
        if (inner == nullptr) {
          errors.emplace_back("'[" + path + "]' must be a section, not a value");
          continue;
        }

        auto key_names = spp::Vec<spp::Str>();
        for (auto const &k : section->Keys) { key_names.emplace_back(spp::Str(k.Name)); }

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
            errors.emplace_back(
              "unknown key '" + inner_key + "' in [" + path + "]" + Suggest(inner_key, key_names)
              + "\n    [" + path + "] accepts: " + (key_names.empty()
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
            : spec->Kind == ValueKind::Version
            ? inner_node.is_string() and IsVersion(*inner_node.template value<spp::Str>())
            : inner_node.is_table();
          if (not ok) {
            errors.emplace_back("'" + inner_key + "' in [" + path + "] must be " + spp::Str(KindName(spec->Kind)));
          }
        }

        for (auto const &k : section->Keys) {
          if (k.Required and not inner->contains(k.Name)) {
            errors.emplace_back("[" + path + "] is missing the required key '" + spp::Str(k.Name) + "'");
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

      errors.emplace_back(
        "unknown section '[" + written + "]'" + Suggest(written, section_names)
        + "\n    known sections: " + [&] {
          auto joined = spp::Str();
          for (auto const &n : section_names) { joined += (joined.empty() ? "" : ", ") + n; }
          return joined;
        }());
    }
  }
}

auto spp::utils::features::Schema() -> Vec<SectionSpec> const& {
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
        KeySpec{
          .Key = ConfigKey::MemoryStackProbe,
          .Name = "probe",
          .Kind = ValueKind::Bool,
          .Required = false,
          .Default = true,
          .Help = "Claim a frame a page at a time, so a large one cannot step over the guard page below the stack."
        },
        KeySpec{
          .Key = ConfigKey::MemoryStackSplit,
          .Name = "split",
          .Kind = ValueKind::Bool,
          .Required = false,
          .Default = true,
          .Help = "Put the objects a callee could overflow on a stack of their own, away from return addresses."
        },
      },
      .FreeForm = false,
    },
    SectionSpec{
      .Name = "memory.map",
      .Help = "Protections applied to the process's memory mappings.",
      .Keys = Vec<KeySpec>{
        KeySpec{
          .Key = ConfigKey::MemoryMapDenyWx,
          .Name = "deny_wx",
          .Kind = ValueKind::Bool,
          .Required = false,
          .Default = true,
          .Help = "Refuse any mapping that is, or later becomes, both writable and executable, for this process "
          "but not its children. Needs Linux 6.6: left out below '[target.floor] linux', and on any other "
          "target; turned on here explicitly, that is a build error instead.",
          .MinLinux = "6.6"
        },
        KeySpec{
          .Key = ConfigKey::MemoryMapSeal,
          .Name = "seal",
          .Kind = ValueKind::Bool,
          .Required = false,
          .Default = true,
          .Help = "Seal the code, constants and relocated data of everything loaded at startup, so none of it can be "
          "unmapped, remapped or made writable again. Needs Linux 6.10: left out below '[target.floor] linux', "
          "and on any other target; turned on here explicitly, that is a build error instead.",
          .MinLinux = "6.10"
        },
      },
      .FreeForm = false,
    },
    SectionSpec{
      .Name = "binary.codegen",
      .Help = "What the code generator is asked to emit around the instructions themselves.",
      .Keys = Vec<KeySpec>{
        KeySpec{
          .Key = ConfigKey::BinaryCodegenSls,
          .Name = "sls",
          .Kind = ValueKind::Bool,
          .Required = false,
          .Default = true,
          .Help = "Put a trap straight after every return and indirect jump, so the bytes after one are never run "
          "speculatively. Code size only. x86 and AArch64; any other target fails the build until this is "
          "turned off."
        },
        KeySpec{
          .Key = ConfigKey::BinaryCodegenScrub,
          .Name = "scrub",
          .Kind = ValueKind::Bool,
          .Required = false,
          .Default = true,
          .Help = "Zero the registers a function used, and its caller does not keep, just before it returns, so "
          "nothing it computed is left behind. A few instructions on every return. x86 and AArch64; any "
          "other target fails the build until this is turned off."
        },
        KeySpec{
          .Key = ConfigKey::BinaryCodegenCfi,
          .Name = "cfi",
          .Kind = ValueKind::Bool,
          .Required = false,
          .Default = true,
          .Help = "Check, before every indirect call (a closure, a function value, resuming or destroying a "
          "generator), that the function it reaches has the s++ signature the call expects, and stop the "
          "program if not. A load and a compare per call. x86 and AArch64; any other target fails the build "
          "until this is turned off."
        },
        KeySpec{
          .Key = ConfigKey::BinaryCodegenJumpTableBounds,
          .Name = "jump_table_bounds",
          .Kind = ValueKind::Bool,
          .Required = false,
          .Default = true,
          .Help = "Keep a switch from jumping through an entry past the end of its jump table, even speculatively. "
          "On AArch64 the index is clamped without a branch; on x86 switches are built without jump tables, "
          "as compares. A few instructions per switch. Any other target fails the build until this is "
          "turned off."
        },
      },
      .FreeForm = false,
    },
    SectionSpec{
      .Name = "binary.link",
      .Help = "What the linker is asked to make of the sections code generation produced.",
      .Keys = Vec<KeySpec>{
        KeySpec{
          .Key = ConfigKey::BinaryLinkHarden,
          .Name = "harden",
          .Kind = ValueKind::Bool,
          .Required = false,
          .Default = true,
          .Help = "Make every section that only startup writes read-only once startup is over, leave the stack "
          "non-executable and the code its own mapping, and fail the link on a text relocation, a "
          "writable-and-executable segment or code that shares a page with data."
        },
        KeySpec{
          .Key = ConfigKey::BinaryLinkFortifiedFfi,
          .Name = "fortified_ffi",
          .Kind = ValueKind::Bool,
          .Required = false,
          .Default = true,
          .Help = "Fail the link against an ffi library that calls, unchecked, a libc function libc has a checked "
          "version of (memcpy, sprintf, printf, ...), as one built without _FORTIFY_SOURCE does. ELF libraries "
          "against glibc only. Rebuild the library with -O2 -D_FORTIFY_SOURCE=3, or turn this off."
        },
        KeySpec{
          .Key = ConfigKey::BinaryLinkRelroFfi,
          .Name = "relro_ffi",
          .Kind = ValueKind::Bool,
          .Required = false,
          .Default = true,
          .Help = "Fail the link against an ffi library linked without full RELRO (a PT_GNU_RELRO segment and "
          "BIND_NOW), whose relocated data, its GOT included, would stay writable after startup where the "
          "program's own does not. ELF libraries only. Relink the library with -Wl,-z,relro,-z,now, or turn "
          "this off."
        },
        KeySpec{
          .Key = ConfigKey::BinaryLinkSegmentsFfi,
          .Name = "segments_ffi",
          .Kind = ValueKind::Bool,
          .Required = false,
          .Default = true,
          .Help = "Fail the link against an ffi library with a writable-and-executable segment, or code that shares "
          "a page with data, which mapping the page executable makes executable too. Checked in the library, so "
          "a linker that ignored the flags is caught. ELF libraries only. Relink the library with "
          "-Wl,-z,separate-code (binutils 2.31 or newer), or turn this off."
        },
      },
      .FreeForm = false,
    },
    SectionSpec{
      .Name = "target.floor",
      .Help = "The oldest system the program must run on. A run-time measure that needs a newer one is left out, "
      "and the program's hardening record says so.",
      .Keys = Vec<KeySpec>{
        KeySpec{
          .Key = ConfigKey::TargetFloorLinux,
          .Name = "linux",
          .Kind = ValueKind::Version,
          .Required = false,
          .Default = false,
          .Help = "The oldest Linux kernel, as 'major.minor'. The default is RHEL 8's, which leaves out deny_wx "
          "(6.6) and seal (6.10); raise it to get them.",
          .DefaultText = "4.18"
        },
      },
      .FreeForm = false,
    },
    SectionSpec{
      .Name = "target.linkers",
      .Help = "The linker for a target, keyed by its folder name (\"s390x-linux-gnu\"): a command, run with the "
      "link's arguments after it. Unnamed, the host links with 'cc', and a Linux target with its GNU cross "
      "compiler, \"<arch>-linux-<env>-gcc\" or the newest versioned one, \"...-gcc-N\".",
      .Keys = Vec<KeySpec>{},
      .FreeForm = true,
    },
  };
  return schema;
}

auto spp::utils::features::Validate(
  Path const &config, Vec<Str> &errors) -> bool {
  // The project has to have an "spp.toml"; without one there is
  // nothing to check, and that is reported.
  if (not std::filesystem::exists(config)) {
    errors.emplace_back("no 'spp.toml' here");
    return false;
  }

  // Parse the file and walk the table, otherwise if the
  // file is bad, report an error. This will be an error
  // with the toml structure / syntax, not the key/value
  // validity based off the schema. WalkTable will throw
  // the semantic errors.
  try {
    const auto parsed = toml::parse_file(utils::files::NativeString(config));
    auto const &root = parsed;
    WalkTable(root, Str(), errors);
  }
  catch (std::exception const &e) {
    errors.emplace_back(Str("could not read 'spp.toml': ") + e.what());
  }

  // Validity of semantics is enforced by the tracking
  // list remaining empty.
  return errors.empty();
}

auto spp::utils::features::Load(
  Path const &config, Vec<Str> &errors) -> bool {
  Reset();
  if (not Validate(config, errors)) { return false; }

  // Prase the toml file and get the current feature set
  // for modification.
  const auto parsed = toml::parse_file(utils::files::NativeString(config));
  auto const &root = parsed;
  auto &flags = CurrentFlags();

  // Iterate through the schema; there will be no key/value
  // pairs in the validated toml (or section names), that
  // don't exist in the schema.
  for (auto const &section : Schema()) {
    // Move through the keys of the section.
    for (auto const &k : section.Keys) {
      if (k.Kind == ValueKind::Table) { continue; }

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
      CurrentSet().insert(k.Key);
      if (k.Kind != ValueKind::Bool) { CurrentTexts()[k.Key] = *value->value<Str>(); }
      else if (const auto b = value->value<bool>()) { flags[k.Key] = *b; }
      else if (const auto i = value->value<std::int64_t>()) { flags[k.Key] = *i != 0; }
    }
  }

  // The linkers, each a command for a target folder.
  if (auto const *const linkers = root["target"]["linkers"].as_table(); linkers != nullptr) {
    for (auto &&[folder, command] : *linkers) {
      const auto text = command.value<Str>();
      if (not text.has_value() or text->empty()) {
        errors.emplace_back("'" + Str(folder.str()) + "' in [target.linkers] must be a command, as a string");
        return false;
      }
      CurrentLinkers()[Str(folder.str())] = *text;
    }
  }

  // Everything loaded.
  return true;
}

auto spp::utils::features::Enabled(
  const ConfigKey key) -> bool {
  // Asked before a project's "spp.toml" was loaded (the unit
  // tests, and anything read before "Load"), the defaults stand.
  // This also fills non-required flag's values with their default.
  if (not FlagsReady()) { ApplyDefaults(); }

  // Get the flags's value, and ensure the flag was actually
  // found in the feature set too.
  const auto it = CurrentFlags().find(key);
  SPP_ASSERT(it != CurrentFlags().end());
  return it->second;
}

auto spp::utils::features::Text(
  const ConfigKey key) -> Str {
  if (not FlagsReady()) { ApplyDefaults(); }
  const auto it = CurrentTexts().find(key);
  return it != CurrentTexts().end() ? it->second : Str();
}

auto spp::utils::features::IsSet(
  const ConfigKey key) -> bool {
  return CurrentSet().contains(key);
}

auto spp::utils::features::LeftOut(
  const ConfigKey key, const bool target_linux) -> Str {
  const auto [_, spec] = FindKey(key);
  if (spec->MinLinux.empty() or not Enabled(key)) { return Str(); }
  if (not target_linux) { return "Linux only, and the target is not Linux"; }
  const auto floor = Text(ConfigKey::TargetFloorLinux);
  if (VersionLess(floor, spec->MinLinux)) {
    return "needs Linux " + Str(spec->MinLinux) + ", above the floor " + floor;
  }
  return Str();
}

auto spp::utils::features::Applied(
  const ConfigKey key, const bool target_linux) -> bool {
  return Enabled(key) and LeftOut(key, target_linux).empty();
}

auto spp::utils::features::CheckFloor(
  const bool target_linux, Vec<Str> &errors) -> bool {
  const auto before = errors.size();
  for (auto const &section : Schema()) {
    for (auto const &k : section.Keys) {
      if (not IsSet(k.Key)) { continue; }
      const auto why = LeftOut(k.Key, target_linux);
      if (why.empty()) { continue; }
      errors.emplace_back(
        "'" + Str(k.Name) + " = true' in [" + Str(section.Name) + "] cannot be applied: " + why
        + (target_linux ? "; raise 'linux' in [target.floor], or turn it off" : "; turn it off"));
    }
  }
  return errors.size() == before;
}

auto spp::utils::features::Reset() -> void {
  // Clear all the values and what was set, and put the defaults in.
  CurrentFlags().clear();
  CurrentTexts().clear();
  CurrentSet().clear();
  CurrentLinkers().clear();
  ApplyDefaults();
}

auto spp::utils::features::Linker(
  const StrView target_folder) -> Opt<Str> {
  const auto it = CurrentLinkers().find(Str(target_folder));
  return it != CurrentLinkers().end() ? Opt<Str>(it->second) : Nullopt;
}

auto spp::utils::features::HelpText() -> Str {
  auto out = Str("Sections and keys accepted in 'spp.toml':\n");
  for (auto const &section : Schema()) {
    out += "\n  [" + Str(section.Name) + "]\n    " + Str(section.Help) + "\n";
    if (section.FreeForm) {
      out += "    (any key)\n";
      continue;
    }
    for (auto const &k : section.Keys) {
      out += "    - " + Str(k.Name);
      out += k.Kind == ValueKind::Bool
        ? Str(" = ") + (k.Default ? "true" : "false") + "  (default)"
        : not k.DefaultText.empty()
        ? Str(" = \"") + Str(k.DefaultText) + "\"  (default)"
        : k.Required
        ? Str("  (required)")
        : Str();
      out += "\n      " + Str(k.Help) + "\n";
    }
  }
  return out;
}

auto spp::utils::features::EffectiveSettings(
  const bool target_linux) -> Str {
  auto out = Str();
  for (auto const &section : Schema()) {
    for (auto const &k : section.Keys) {
      auto value = Str();
      if (k.Kind == ValueKind::Version) { value = Text(k.Key); }
      else if (k.Kind != ValueKind::Bool) { continue; }
      else if (const auto why = LeftOut(k.Key, target_linux); not why.empty()) { value = "left out (" + why + ")"; }
      else { value = Enabled(k.Key) ? "true" : "false"; }
      out += Str(section.Name) + "." + Str(k.Name) + " = " + value + "\n";
    }
  }
  return out;
}
