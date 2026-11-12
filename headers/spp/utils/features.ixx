module;
#include <spp/macros.hpp>

export module spp.utils.features;
import spp.utils.types;
import std;

namespace spp::utils::features {
  /// What kind a value on a key is. The booleans are for "flags" - is xyz
  /// enabled; the strings are for options - the linker is "xyz"; the tables are
  /// for example the vcs entries; the version is just for the package version.
  SPP_EXP_CLS enum class ValueKind { Bool, String, Version, Table };

  /// The available config keys where their value tunes some area in the
  /// compiler - for example, reading the memory hardening flags to enable
  /// certain features in LLVM etc.
  SPP_EXP_CLS enum class ConfigKey {
    ProjectName,
    ProjectVersion,
    ProjectBuild,
    MemoryStackProtect,
    MemoryStackProbe,
    MemoryStackSplit,
    MemoryMapDenyWx,
    MemoryMapSeal,
    BinaryCodegenSls,
    BinaryCodegenScrub,
    BinaryCodegenCfi,
    BinaryCodegenBti,
    BinaryCodegenPacRet,
    BinaryCodegenJumpTableBounds,
    BinaryCodegenFfiUnwindGuard,
    BinaryCodegenAirIntegers,
    BinaryLinkHarden,
    BinaryLinkFortifiedFfi,
    BinaryLinkRelroFfi,
    BinaryLinkSegmentsFfi,
    BinaryDebugInfo,
    TargetFloorLinux,
    TargetFloorCpu,
  };

  /// A key specification contains the metadata of a key within a section,
  /// describing its expected type, default value, if it is required, help text,
  /// etc. Importantly, the "minimum linux" which is a floor for the key.
  SPP_EXP_CLS struct KeySpec {
    ConfigKey Key;
    StrView Name;
    ValueKind Kind;
    bool Required;
    bool Default; /// Default for bool values (feature enabled or not).
    StrView DefaultText = {}; /// Default for string and version values.
    StrView Help;
    StrView MinLinux = {}; /// Empty for one that needs nothing of the kernel.
    Vec<StrView> Choices = {}; /// The values a string key may take; empty for any.
  };

  /// How much debug info a program is built with ("[binary.debug] info"):
  /// none, or the line tables that map its code back to its source.
  SPP_EXP_CLS enum class DebugInfo { None, Lines, Full };

  /// A section specification contains the metadata of a section within the
  /// schema, including its name, list of keys (as key specifications) and if
  /// the section is "free form" (ie [vcs] can have free form entries).
  SPP_EXP_CLS struct SectionSpec {
    StrView Name;
    StrView Help;
    Vec<KeySpec> Keys;
    bool FreeForm;
  };

  /// What the selected target can do, read from codegen by the caller. Features
  /// sits below codegen, so it is handed these rather than asking for them.
  SPP_EXP_CLS struct TargetFacts {
    bool SafeStack;
    bool SafeStackCanary;
    bool PacRet;
    bool IsLinux;
    StrView Folder;
  };

  /// Every section and the key the compiler knows, which is what an
  /// unrecognised one is reported against.
  SPP_EXP_FUN auto Schema() -> Vec<SectionSpec> const&;

  /// Check the config against the schema, reporting anything unrecognised or
  /// missing. If an invalid section name is found, all the valid section names
  /// are reported. If, within a valid section, an invalid key is found, then
  /// all the valid keys for that section are reported.
  SPP_EXP_FUN auto Validate(Path const &config, Vec<Str> &errors) -> bool;

  /// Load a config and load the values from the toml settings into the feature
  /// system within C++. This is a single place to dynamically set all the
  /// feature values.
  SPP_EXP_FUN auto Load(Path const &config, Vec<Str> &errors) -> bool;

  /// Check whether a feature is enabled, either from the defaults or the config
  /// overrides, and not left out by "LeaveOut".
  SPP_EXP_FUN auto IsEnabled(ConfigKey key) -> bool;

  /// Whether a measure goes into the program: turned on, and not left out
  /// because of incompatibilities, or by "LeaveOut".
  SPP_EXP_FUN auto IsApplied(ConfigKey key, bool target_linux) -> bool;

  /// Whether the project's "spp.toml" sets the key itself, rather than leaving
  /// it to its default: a measure set on explicitly is refused where it cannot
  /// be had, where its default is only left out.
  SPP_EXP_FUN auto IsExplicit(ConfigKey key) -> bool;

  /// Leave out a measure the build found it cannot have (the toolchain cannot
  /// keep it), saying why: it is not applied, and the record reads "left out
  /// (why)". Only for one on by default; one set on explicitly is refused
  /// instead. Cleared by "Reset".
  SPP_EXP_FUN auto LeaveOut(ConfigKey key, StrView why) -> void;

  /// The value of a string or version key, set or defaulted.
  SPP_EXP_FUN auto Text(ConfigKey key) -> Str;

  /// "[binary.debug] info": as spp.toml sets it, else full debug info for a
  /// "dev" build and line tables for a "rel" one.
  SPP_EXP_FUN auto DebugInfoLevel(bool dev) -> DebugInfo;

  /// Report every flag that the config has been manually set that trips the
  /// "IsFlagIncompatibleWithSystem" function, with error wrapping. Asking for a
  /// feature the floor or the target cannot have is an error, not ignores or
  /// quietly downgraded.
  SPP_EXP_FUN auto CheckFloor(
    TargetFacts const &target, Vec<Str> &errors) -> bool;

  /// Refuse a measure the target cannot have, on by default or not. Building
  /// without it would quietly revert hardening.
  SPP_EXP_FUN auto CheckTargetSupport(
    TargetFacts const &target, Vec<Str> &errors) -> bool;

  /// The linker "[target.linkers]" names for a target folder, if it names one:
  /// a command, run with the link's arguments after it.
  SPP_EXP_FUN auto Linker(StrView target_folder) -> Opt<Str>;

  /// Reset every feature to its default - as if the compiler never read the
  /// config file for overrides at all.
  SPP_EXP_FUN auto Reset() -> void;

  /// The entire schema as a listing, for the command "spp config", and for the
  /// tail of a validation failure.
  SPP_EXP_FUN auto HelpText() -> Str;

  /// Every on/off and version key and the value it has for this build, default
  /// or set, as "section.key = value" lines in schema order: what a built
  /// program records about itself. A measure that is on but left out reads
  /// "left out (why)".
  SPP_EXP_FUN auto EffectiveSettings(bool target_linux) -> Str;
}
