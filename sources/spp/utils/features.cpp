module;
#include <spp/macros.hpp>

module spp.utils.features;
import spp.utils.files;
import spp.utils.ice;
import spp.utils.strings;
import genex;
import tomlpp;

namespace spp::utils::features {
  namespace {
    /// [CHECKED]
    /// The settings currently applied, populated from the schema's defaults and
    /// then from the config. The map is structured as {"feature" : "enabled"},
    /// and the static instance is borrowed and mutated.
    auto CurrentFlags() -> auto& {
      static auto flags = Map<ConfigKey, bool>();
      return flags;
    }

    /// [CHECKED]
    /// The values of string and version keys, defaulted from the schema and
    /// also borrowed and mutated.
    auto CurrentTexts() -> auto& {
      static auto texts = Map<ConfigKey, Str>();
      return texts;
    }

    /// [CHECKED]
    /// Every key the loaded config sets itself, whatever its value. Effectively
    /// all the overridden keys.
    auto CurrentSet() -> auto& {
      static auto set = Set<ConfigKey>();
      return set;
    }

    /// [CHECKED]
    /// The linkers "[target.linkers]" names, by target folder.
    auto CurrentLinkers() -> auto& {
      static auto linkers = Map<Str, Str>();
      return linkers;
    }

    /// The measures this build found it cannot have (the toolchain cannot keep
    /// them), with why: on by default, so left out rather than refused.
    auto CurrentLeftOut() -> auto& {
      static auto left_out = Map<ConfigKey, Str>();
      return left_out;
    }

    /// [CHECKED]
    /// Whether the defaults have been put in place yet, so that a query before
    /// any load still answers correctly. Once set to true, it will always be
    /// true until the program ends.
    auto FlagsReady() -> auto& {
      static auto ready = false;
      return ready;
    }

    /// [CHECKED]
    /// Whether the loaded config sets the key itself, rather than leaving it to
    /// its default.
    auto IsManuallySet(const ConfigKey key) -> bool {
      return CurrentSet().contains(key);
    }

    /// Return the key, by "key" from the schema. This is a "key spec", and
    /// contains associated key information.
    auto FindKey(const ConfigKey key) -> KeySpec const* {
      for (auto const &section : Schema()) {
        for (auto const &k : section.Keys) {
          if (k.Key == key) { return &k; }
        }
      }
      Ice("config key missing from the schema");
    }

    /// [CHECKED]
    /// Return the section, by "name" from the schema. This is a "section spec",
    /// and contains allowed keys, helptext etc.
    auto FindSection(const StrView name) -> SectionSpec const* {
      for (auto const &section : Schema()) {
        if (section.Name == name) { return &section; }
      }
      return nullptr; // Todo: Ice?
    }

    /// [CHECKED] [TIDY]
    /// Whether version "a" is older than version "b", compared part by part, a
    /// missing part being 0.
    auto VersionLess(const StrView a, const StrView b) -> bool {
      // Define the helper to convert the version into an extended integer. For
      // example, "1.2.3" becomes [1, 0, 2, 0, 3]
      const auto parts = [](const StrView v) {
        auto out = Vec<std::uint64_t>{0};
        for (const auto c : v) {
          if (c == '.') { out.emplace_back(0); }
          else {
            out.back() = out.back() * 10 + static_cast<std::uint64_t>(c - '0');
          }
        }
        return out;
      };

      // Convert the two versions into vectors of numbers, and get the largest
      // size.
      auto pa = parts(a);
      auto pb = parts(b);
      const auto size = std::max(pa.size(), pb.size());

      // Resize the number vectors to the largest size, with "0" as the
      // placeholder, then do a normal comparison.
      pa.resize(size, 0);
      pb.resize(size, 0);
      return pa < pb;
    }

    /// [CHECKED]
    /// Why a run-time measure that is turned on manually, must actually be off:
    /// it needs a newer Linux than "[target.floor] linux", or the target is not
    /// Linux.
    auto IsFlagIncompatibleWithSystem(
      const ConfigKey key, const bool target_linux) -> Pair<bool, Str> {
      // Find the key specification from the schema/section based off of the key
      // name (as enum entry). If there is no linux base, or the key is not
      // enabled, then return (never a compatibility conflict).
      const auto spec = FindKey(key);
      if (spec->MinLinux.empty() or not IsEnabled(key)) { return {false, Str()}; }

      // If we have a non-linux target, then this is the reason the flag must be
      // disabled. Return the error.
      if (not target_linux) {
        return {true, "Linux only, and the target is not Linux"};
      }

      // If the current linux version is lower than the floor for that key, then
      // that is the error.
      const auto floor = Text(ConfigKey::TargetFloorLinux);
      if (VersionLess(floor, spec->MinLinux)) {
        return {
          true,
          "needs Linux " + Str(spec->MinLinux) + ", above the floor " + floor
        };
      }
      return {false, Str()};
    }

    /// [CHECKED]
    /// Set all the feature flags to their default values, and mark all flags as
    /// "ready".
    auto ApplyDefaults() -> void {
      auto &flags = CurrentFlags();
      auto &texts = CurrentTexts();

      // Iterate the full possible schema, and then within each schema section,
      // the set of possible keys.
      for (auto const &section : Schema()) {
        for (auto const &k : section.Keys) {
          if (k.Kind == ValueKind::Bool) { flags[k.Key] = k.Default; }
          if (not k.DefaultText.empty()) { texts[k.Key] = Str(k.DefaultText); }
        }
      }
      FlagsReady() = true;
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

    /// Walk the config, reporting anything the schema does not describe, into
    /// the "errors" vector.
    auto WalkTable(
      auto const &table, Str const &prefix, Vec<Str> &errors) -> void {
      // Every section name, and every one that is a section or a step on the
      // way to one, so that "[memory]" on its own is a container rather than a
      // mistake.
      auto section_names = spp::Vec<Str>();
      for (auto const &section : Schema()) {
        section_names.emplace_back(Str(section.Name));
      }

      // Iterate all the values in the provided config. The key is grabbed per
      // row and the prefix prepended, should it exist. The prefix from
      // inspecting inner blocks recursively (from this function).
      for (auto &&[raw_key, node] : table) {
        const auto key = Str(raw_key.str());
        const auto path = prefix.empty() ? key : prefix + "." + key;

        if (const auto section_sp = FindSection(path); section_sp != nullptr) {
          // For a section, every key present must be in the schema (section
          // spec), and all the section's required keys must be present.
          const auto inner = node.as_table();
          if (inner == nullptr) {
            errors.emplace_back(
              "'[" + path + "]' must be a section, not a value");
            continue;
          }

          // Record the valid keys for the section spec. These will be used to
          // detect invalid keys.
          auto key_names = spp::Vec<Str>();
          for (auto const &k : section_sp->Keys) {
            key_names.emplace_back(Str(k.Name));
          }

          for (auto &&[raw_inner_key, inner_node] : *inner) {
            // Get the "key spec" for each user provided key, by iterating the
            // section spec and checking for a matching key name.
            const auto inner_key = Str(raw_inner_key.str());
            auto key_sp = static_cast<KeySpec const*>(nullptr);
            for (auto const &k : section_sp->Keys) {
              if (k.Name == inner_key) {
                key_sp = &k;
                break;
              }
            }

            // If the key is nullptr, then nothing in the section spec matched
            // it. This is an error, unless the section accepts free form args.
            if (key_sp == nullptr) {
              if (section_sp->FreeForm) { continue; }
              errors.emplace_back(
                "unknown key '" + inner_key + "' in [" + path + "]"
                + Suggest(inner_key, key_names)
                + "\n    [" + path + "] accepts: " + (key_names.empty()
                  ? Str("(nothing)")
                  : [&] {
                    auto joined = Str();
                    for (auto const &n : key_names) {
                      joined += (joined.empty() ? "" : ", ") + n;
                    }
                    return joined;
                  }()));
              continue;
            }

            // Do a type check on the value: bool, string and version checks,
            // otherwise a table itself.
            const auto ok =
              key_sp->Kind == ValueKind::Bool
              ? inner_node.is_boolean()
              : key_sp->Kind == ValueKind::String
              ? inner_node.is_string()
              : key_sp->Kind == ValueKind::Version
              ? inner_node.is_string() and IsVersion(
                *inner_node.template value<Str>())
              : inner_node.is_table();
            if (not ok) {
              errors.emplace_back(
                "'" + inner_key + "' in [" + path + "] must be "
                + Str(KindName(key_sp->Kind)));
            }
          }

          // If nothing in the user provided section gave a value for a required
          // key, throw an error.
          for (auto const &k : section_sp->Keys) {
            if (k.Required and not inner->contains(k.Name)) {
              errors.emplace_back(
                "[" + path + "] is missing the required key '"
                + Str(k.Name) + "'");
            }
          }
          continue;
        }

        // Not a section itself. It is fine only as a step towards one, and only
        // if it is a table to step into.
        const auto is_prefix = genex::any_of(section_names, [&](Str const &n) {
          return n.size() > path.size()
            and n.starts_with(path)
            and n[path.size()] == '.';
        });
        if (is_prefix and node.is_table()) {
          WalkTable(*node.as_table(), path, errors);
          continue;
        }

        // Report the path as it was written. Toml nests, so "[memry.stack]"
        // arrives as an unknown "memry" holding a "stack", and naming only the
        // outer half describes something the author never typed.
        auto written = path;
        auto walk = node.as_table();
        while (walk != nullptr and walk->size() == 1) {
          auto descend = static_cast<toml::table const*>(nullptr);
          for (auto &&[only_key, only_node] : *walk) {
            if (not only_node.is_table()) { break; }
            written += "." + Str(only_key.str());
            descend = only_node.as_table();
          }
          walk = descend;
        }

        errors.emplace_back(
          "unknown section '[" + written + "]'"
          + Suggest(written, section_names)
          + "\n    known sections: " + [&] {
            auto joined = Str();
            for (auto const &n : section_names) {
              joined += (joined.empty() ? "" : ", ") + n;
            }
            return joined;
          }());
      }
    }
  }
}

auto spp::utils::features::Schema() -> Vec<SectionSpec> const& {
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
          .Key = ConfigKey::BinaryCodegenBti,
          .Name = "bti",
          .Kind = ValueKind::Bool,
          .Required = false,
          .Default = true,
          .Help = "Start every function, and every block an indirect jump can reach, with a landing pad, and have "
          "the processor fault on an indirect branch to anywhere else (BTI). One instruction per function. "
          "AArch64 Linux and FreeBSD; elsewhere 'cfi' stands in for it (x86-64, macOS and Windows on AArch64), "
          "and a target without either fails the build until this is turned off."
        },
        KeySpec{
          .Key = ConfigKey::BinaryCodegenPacRet,
          .Name = "pac_ret",
          .Kind = ValueKind::Bool,
          .Required = false,
          .Default = true,
          .Help = "Sign every return address a function saves to the stack, and stop the program when one comes back "
          "altered (pointer authentication). Two instructions per function that calls another; no-ops on a "
          "processor without it. AArch64 Linux and FreeBSD; elsewhere 'split' in [memory.stack] stands in for "
          "it, keeping what could overflow away from return addresses, and a target without either fails the "
          "build until this is turned off."
        },
        KeySpec{
          .Key = ConfigKey::BinaryCodegenJumpTableBounds,
          .Name = "jump_table_bounds",
          .Kind = ValueKind::Bool,
          .Required = false,
          .Default = true,
          .Help = "Keep a switch from jumping through an entry past the end of its jump table, even speculatively. "
          "A dense switch gets a table whose index is masked into range without a branch (none when the value "
          "provably fits); every other switch is built as compares. Every target; WebAssembly, which has no "
          "indirect jump, builds every switch as compares. No measurable cost against an unprotected table."
        },
        KeySpec{
          .Key = ConfigKey::BinaryCodegenFfiUnwindGuard,
          .Name = "ffi_unwind_guard",
          .Kind = ValueKind::Bool,
          .Required = false,
          .Default = true,
          .Help = "Stop the program, naming the function, when an exception from foreign code unwinds into s++, "
          "rather than letting it skip the s++ frames' defers and linear values on its way to a C++ handler. "
          "Unwind-table data only. m68k fails the build until this is turned off: LLVM writes its unwind "
          "tables' personality and LSDA pointers as absolute addresses, which a position-independent "
          "executable cannot hold."
        },
        KeySpec{
          .Key = ConfigKey::BinaryCodegenAirIntegers,
          .Name = "air_integers",
          .Kind = ValueKind::Bool,
          .Required = false,
          .Default = true,
          .Help = "Integer overflow is checked as if integers were infinitely ranged: the program stops before a "
          "wrong value is used (as an index, size, divisor, branch, output and so on) rather than at the operation, "
          "so checks can be moved where they cost least and loops vectorised. Every overflow that matters still "
          "stops it, naming the check; the report is where the value was used. Off, every check is made at its "
          "operation. rel only; dev always checks at the operation."
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
          .DefaultText = "4.18",
          .Help = "The oldest Linux kernel, as 'major.minor'. The default is RHEL 8's, which leaves out deny_wx "
          "(6.6) and seal (6.10); raise it to get them."
        },
        KeySpec{
          .Key = ConfigKey::TargetFloorCpu,
          .Name = "cpu",
          .Kind = ValueKind::String,
          .Required = false,
          .Default = false,
          .DefaultText = "generic",
          .Help = "The oldest CPU the program must run on, which code generation may use every instruction of: "
          "'generic' (the target's baseline, which runs anywhere the target does), 'native' (the CPU of the machine "
          "building it, when building for that machine), or an LLVM CPU name for the target, such as 'x86-64-v3' "
          "(AVX2) or 'apple-m1'. The program stops with an illegal instruction on an older CPU."
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

/// [CHECKED]
auto spp::utils::features::Validate(
  Path const &config, Vec<Str> &errors) -> bool {
  // The project has to have an "spp.toml"; without one there is nothing to
  // check, and that is reported (prevent downstream errors).
  if (not std::filesystem::exists(config)) {
    errors.emplace_back("no 'spp.toml' here");
    return false;
  }

  // Parse the file and walk the table, and if the file is bad, report an error.
  // This will be an error with the toml structure / syntax, not the key/value
  // validity based off the schema. WalkTable will throw the semantic errors.
  try {
    const auto parsed = toml::parse_file(files::NativeString(config));
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

/// [CHECKED]
auto spp::utils::features::Load(Path const &config, Vec<Str> &errors) -> bool {
  // Reset the loaded features and options, and then perform a validation on the
  // config. If it fails, return early.
  Reset();
  if (not Validate(config, errors)) { return false; }

  // Parse the toml file and get the current feature set for modification.
  const auto root = toml::parse_file(files::NativeString(config));
  auto &flags = CurrentFlags();
  auto &texts = CurrentTexts();
  auto &linkers = CurrentLinkers();

  // Iterate through the schema; there will be no key/value pairs in the
  // validated toml (or section names), that don't exist in the schema.
  for (auto const &section : Schema()) {
    for (auto const &k : section.Keys) {
      if (k.Kind == ValueKind::Table) { continue; }

      // The section's name is dotted, so it is walked a step at a time rather
      // than looked up whole.
      auto node = static_cast<toml::node const*>(&root);
      auto rest = StrView(section.Name);
      while (node != nullptr and not rest.empty()) {
        const auto dot = rest.find('.');
        const auto part = Str(rest.substr(0, dot));
        rest = dot == StrView::npos ? StrView() : rest.substr(dot + 1);
        auto const *const as_table = node->as_table();
        node = as_table != nullptr and as_table->contains(part)
          ? as_table->get(part)
          : nullptr;
      }

      // A nullptr node means that the section from the schema wasn't present in
      // the toml (not all are required), so continue to the next key/section.
      if (node == nullptr) { continue; }

      // Check that the key is present in the table. If it isn't, then continue,
      // as this key wasn't required.
      const auto as_table = node->as_table();
      if (as_table == nullptr or not as_table->contains(k.Name)) {
        continue;
      }

      // Enforce "false" and "true" are used, not "0" and "1", because we are
      // aiming for single-styled, uniform config files. Semantically there's no
      // difference, just enforced styling.
      const auto value = as_table->get(k.Name);
      CurrentSet().insert(k.Key);
      if (k.Kind != ValueKind::Bool) { texts[k.Key] = *value->value<Str>(); }
      else if (const auto b = value->value<bool>()) { flags[k.Key] = *b; }
      else if (const auto i = value->value<std::int64_t>()) {
        flags[k.Key] = *i != 0;
      }
    }
  }

  // The linkers, each a command for a target folder.
  const auto linker_nodes = root["target"]["linkers"].as_table();
  if (linker_nodes != nullptr) {
    for (auto &&[folder, command] : *linker_nodes) {
      const auto text = command.value<Str>();
      if (not text.has_value() or text->empty()) {
        errors.emplace_back(
          "'" + Str(folder.str())
          + "' in [target.linkers] must be a command, as a string");
        return false;
      }
      linkers[Str(folder.str())] = *text;
    }
  }

  // Everything loaded.
  return true;
}

/// [CHECKED]
auto spp::utils::features::IsEnabled(const ConfigKey key) -> bool {
  // Asked before a project's "spp.toml" was loaded (the unit tests, and
  // anything read before "Load"), the defaults stand. This also fills
  // non-required flag's values with their default.
  if (not FlagsReady()) { ApplyDefaults(); }

  // Get the flags's value, and ensure the flag was actually found in the
  // feature set too.
  const auto it = CurrentFlags().find(key);
  SPP_ASSERT(it != CurrentFlags().end());
  return it->second;
}

/// [CHECKED]
auto spp::utils::features::IsApplied(
  const ConfigKey key, const bool target_linux) -> bool {
  // A flag is applied if it is enabled, and not incompatible with the system
  // constraints.
  return IsEnabled(key)
    and not IsFlagIncompatibleWithSystem(key, target_linux).first
    and not CurrentLeftOut().contains(key);
}

auto spp::utils::features::IsExplicit(
  const ConfigKey key) -> bool {
  return IsManuallySet(key);
}

auto spp::utils::features::LeaveOut(
  const ConfigKey key, const StrView why) -> void {
  CurrentLeftOut().insert_or_assign(key, Str(why));
}

/// [CHECKED]
auto spp::utils::features::Text(
  const ConfigKey key) -> Str {
  // Apply the defaults if the flags aren't ready, otherwise get the text
  // assigned to it.
  if (not FlagsReady()) { ApplyDefaults(); }
  const auto it = CurrentTexts().find(key);
  return it != CurrentTexts().end() ? it->second : Str();
}

/// [CHECKED]
auto spp::utils::features::CheckFloor(
  TargetFacts const &target, Vec<Str> &errors) -> bool {
  const auto before = errors.size();
  // Iterate every section of the schema, and every key per section, to detect
  // any flags that cannot be used because of the floor.
  for (auto const &section : Schema()) {
    for (auto const &k : section.Keys) {

      // Skip any non-manually-set flags (the defaulted ones are already
      // platform-considerate).
      if (not IsManuallySet(k.Key)) { continue; }

      // If there is not a system incompatibility with the key and target
      // system, then skip too.
      const auto [is_flag_incompatible, why] =
        IsFlagIncompatibleWithSystem(k.Key, target.IsLinux);
      if (not is_flag_incompatible) { continue; }

      // Otherwise, the flag is system-incompatible, so capture the error, and
      // wrap it with the key name, adding the whole thing into the error list.
      errors.emplace_back(
        "'" + Str(k.Name) + " = true' in [" + Str(section.Name)
        + "] cannot be applied: " + why
        + (target.IsLinux
          ? "; raise 'linux' in [target.floor], or turn it off"
          : "; turn it off"));
    }
  }

  // Only if the number of errors has incremented, did this function record any
  // new errors.
  return errors.size() == before;
}

/// [CHECKED]
auto spp::utils::features::CheckTargetSupport(
  TargetFacts const &target, Vec<Str> &errors) -> bool {
  if (IsEnabled(ConfigKey::MemoryStackSplit) and not target.SafeStack) {
    errors.emplace_back(
      "'split' in [memory.stack] cannot be applied: "
      + Str("LLVM has no unsafe stack pointer for ")
      + Str(target.Folder) + "; set 'split = false'");
    return false;
  }

  // Todo: drop once LLVM's safestack checks a canary the target's own way; .triages/upstream-watch.md U15.
  if (IsEnabled(ConfigKey::MemoryStackProtect)
    and IsEnabled(ConfigKey::MemoryStackSplit)
    and not target.SafeStackCanary) {
    errors.emplace_back(
      "'protect' and 'split' in [memory.stack] cannot both be applied: "
      + Str("LLVM's split stack checks its canary with __stack_chk_fail; ")
      + Str(target.Folder) + " does not have this. Set one to false");
    return false;
    }
  return true;
}

/// [CHECKED]
auto spp::utils::features::Linker(const StrView target_folder) -> Opt<Str> {
  // From the current linkers, find the target's one (key), and return the
  // associated value.
  const auto it = CurrentLinkers().find(
    Str(target_folder));
  return it != CurrentLinkers().end() ? Opt(it->second) : Nullopt;
}

/// [CHECKED]
auto spp::utils::features::Reset() -> void {
  // Clear all the values and what was set, and put the defaults in. This will
  // also marks the flags as ready.
  CurrentFlags().clear();
  CurrentTexts().clear();
  CurrentSet().clear();
  CurrentLinkers().clear();
  CurrentLeftOut().clear();
  ApplyDefaults();
}
/// [CHECKED]
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

/// [CHECKED]
auto spp::utils::features::EffectiveSettings(
  const bool target_linux) -> Str {
  auto out = Str();
  for (auto const &section : Schema()) {
    for (auto const &k : section.Keys) {
      auto value = Str();
      if (k.Kind == ValueKind::Version) { value = Text(k.Key); }
      else if (k.Kind != ValueKind::Bool) { continue; }
      else if (const auto [is_incompatible_flag, why] =
          IsFlagIncompatibleWithSystem(k.Key, target_linux);
        is_incompatible_flag) { value = "left out (" + why + ")"; }
      else if (const auto it = CurrentLeftOut().find(k.Key); it != CurrentLeftOut().end()) {
        value = "left out (" + it->second + ")";
      }
      else { value = IsEnabled(k.Key) ? "true" : "false"; }
      out += Str(section.Name) + "." + Str(k.Name) + " = " + value + "\n";
    }
  }
  return out;
}
