module;
#include <spp/macros.hpp>

export module spp.utils.features;
import spp.utils.types;
import std;

/**
 * The schema of "spp.toml", and the compiler behaviour some of it selects.
 *
 * @n
 * Two things live here because they are the same thing seen from two ends. One is a description of every section and
 * key a project file may contain, which is what makes an unrecognised one reportable rather than silently ignored -
 * a mistyped key that turns a protection off by doing nothing is worse than no key at all. The other is the answer to
 * "is this protection on", which the compiler asks while it generates code.
 *
 * @n
 * Every protection defaults to on. A project says so explicitly to turn one off, and the schema is what makes that
 * spelling checkable.
 */
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
    bool Default; // Default for bool values (feature enabled or not).
    StrView Help;
  };

  SPP_EXP_CLS struct SectionSpec {
    StrView Name; // [memory.stack]
    StrView Help;
    Vec<KeySpec> Keys;
    bool FreeForm; // Allow "any" key? like in [vcs], the lib names cannot be pre-known.
  };

  /**
   * Every section and key the compiler knows, which is what an unrecognised one is reported against.
   */
  SPP_EXP_FUN auto Schema() -> Vec<SectionSpec> const&;

  /**
   * Check @p config against @c Schema , reporting anything unrecognised or missing. If an invalid section name is
   * found, all the valid section names are reported. If an invalid key is found within a section, all the valid keys
   * from that section are reported.
   * @param config The path of the "spp.toml" to read.
   * @param[out] errors One line per problem, empty when there are none.
   * @return @c true if the file matches the schema.
   */
  SPP_EXP_FUN auto Validate(std::filesystem::path const &config, Vec<Str> &errors) -> bool;

  /**
   * Check @p config and load the values from the toml settings into the feature system within C++. This is a single
   * place to dynamically set all the feature values.
   * @param config The path of the "spp.toml" to read.
   * @param[out] errors One line per problem, empty when there are none.
   * @return @c true if the file matched the schema and the settings were taken.
   */
  SPP_EXP_FUN auto Load(std::filesystem::path const &config, Vec<Str> &errors) -> bool;

  /**
   * Whether the feature @p key is on, which is what it defaults to unless the project turned it off.
   * @param key Must name a @c Bool key in the schema; anything else is a mistake in the compiler rather than in a
   * project, and asserts.
   */
  SPP_EXP_FUN auto Enabled(ConfigKey key) -> bool;

  /**
   * Reset every feature to its default, which is what a compiler that never read a config generates against.
   */
  SPP_EXP_FUN auto Reset() -> void;

  /**
   *The whole schema as a listing, for @c "spp config" and for the tail of a validation failure.
   */
  SPP_EXP_FUN auto HelpText() -> Str;
}
