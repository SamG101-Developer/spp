module;
#include <spp/macros.hpp>

export module spp.utils.features;
import spp.utils.types;
import std;

namespace spp::utils::features {
  SPP_EXP_CLS enum class ValueKind { Bool, String, Version, Table };

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
    BinaryCodegenJumpTableBounds,
    BinaryLinkHarden,
    BinaryLinkFortifiedFfi,
    BinaryLinkRelroFfi,
    BinaryLinkSegmentsFfi,
    TargetFloorLinux,
  };

  SPP_EXP_CLS struct KeySpec {
    ConfigKey Key;

    StrView Name;

    ValueKind Kind;

    bool Required;

    /// Default for bool values (feature enabled or not).
    bool Default;

    StrView Help;

    /// Default for string and version values.
    StrView DefaultText = {};

    /// The oldest Linux kernel a run-time measure works on; empty
    /// for one that needs nothing of the kernel. A measure with
    /// one is Linux-only, and left out on any other target.
    StrView MinLinux = {};
  };

  SPP_EXP_CLS struct SectionSpec {
    /// Example name: "[memory.stack]"
    StrView Name;

    StrView Help;

    Vec<KeySpec> Keys;

    /// Allow "any" key? like in [vcs], the lib names cannot be pre-known.
    bool FreeForm;
  };

  /// Every section and the key the compiler knows, which is
  /// what an unrecognised one is reported against.
  SPP_EXP_FUN auto Schema() -> Vec<SectionSpec> const&;

  /// Check the config against the schema, reporting anything
  /// unrecognised or missing. If an invalid section name is
  /// found, all the valid section names are reported. If,
  /// within a valid section, an invalid key is found, then all
  /// the valid keys for that section are reported.
  SPP_EXP_FUN auto Validate(Path const &config, Vec<Str> &errors) -> bool;

  /// Load a config and load the values from the toml settings
  /// into the feature system within C++. This is a single
  /// place to dynamically set all the feature values.
  SPP_EXP_FUN auto Load(Path const &config, Vec<Str> &errors) -> bool;

  /// Check whether a feature is enabled, either from the
  /// defaults, or the config overrides.
  SPP_EXP_FUN auto Enabled(ConfigKey key) -> bool;

  /// The value of a string or version key, set or defaulted.
  SPP_EXP_FUN auto Text(ConfigKey key) -> Str;

  /// Whether the loaded config sets the key itself, rather than
  /// leaving it to its default.
  SPP_EXP_FUN auto IsSet(ConfigKey key) -> bool;

  /// Why a run-time measure that is turned on is left out of the
  /// program: it needs a newer Linux than "[target.floor] linux",
  /// or the target is not Linux. Empty when it goes in, or is
  /// turned off.
  SPP_EXP_FUN auto LeftOut(ConfigKey key, bool target_linux) -> Str;

  /// Whether a measure goes into the program: turned on, and not
  /// left out.
  SPP_EXP_FUN auto Applied(ConfigKey key, bool target_linux) -> bool;

  /// Report every measure the config turns on itself that
  /// "LeftOut" would leave out: asking for one the floor or the
  /// target cannot have is a mistake, not a quiet downgrade.
  SPP_EXP_FUN auto CheckFloor(bool target_linux, Vec<Str> &errors) -> bool;

  /// The linker "[target.linkers]" names for a target folder, if
  /// it names one: a command, run with the link's arguments after
  /// it.
  SPP_EXP_FUN auto Linker(StrView target_folder) -> Opt<Str>;

  /// Reset every feature to its default - as if the compiler
  /// never read the config file for overrides at all.
  SPP_EXP_FUN auto Reset() -> void;

  /// The entire schema as a listing, for the command "spp
  /// config", and for the tail of a validation failure.
  SPP_EXP_FUN auto HelpText() -> Str;

  /// Every on/off and version key and the value it has for this
  /// build, default or set, as "section.key = value" lines in
  /// schema order: what a built program records about itself, so
  /// a key added to the schema is recorded with no more work. A
  /// measure that is on but left out reads "left out (why)".
  SPP_EXP_FUN auto EffectiveSettings(bool target_linux) -> Str;
}
