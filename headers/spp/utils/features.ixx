module;
#include <spp/macros.hpp>

export module spp.utils.features;
import spp.utils.types;
import std;

namespace spp::utils::features {
  SPP_EXP_CLS enum class ValueKind { Bool, String, Table };

  SPP_EXP_CLS enum class ConfigKey {
    ProjectName,
    ProjectVersion,
    ProjectBuild,
    MemoryStackProtect,
    MemoryStackProbe,
    MemoryStackSplit,
    BinaryLinkHarden,
  };

  SPP_EXP_CLS struct KeySpec {
    ConfigKey Key;

    StrView Name;

    ValueKind Kind;

    bool Required;

    /// Default for bool values (feature enabled or not).
    bool Default;

    StrView Help;
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
  SPP_EXP_FUN auto Validate(std::filesystem::path const &config, Vec<Str> &errors) -> bool;

  /// Load a config and load the values from the toml settings
  /// into the feature system within C++. This is a single
  /// place to dynamically set all the feature values.
  SPP_EXP_FUN auto Load(std::filesystem::path const &config, Vec<Str> &errors) -> bool;

  /// Check whether a feature is enabled, either from the
  /// defaults, or the config overrides.
  SPP_EXP_FUN auto Enabled(ConfigKey key) -> bool;

  /// Reset every feature to its default - as if the compiler
  /// never read the config file for overrides at all.
  SPP_EXP_FUN auto Reset() -> void;

  /// The entire schema as a listing, for the command "spp
  /// config", and for the tail of a validation failure.
  SPP_EXP_FUN auto HelpText() -> Str;
}
