module;
#include <toml++/toml.hpp>

export module tomlpp;

export namespace toml {
  using ::toml::parse_file;
  using ::toml::node;
  using ::toml::table;
  using ::toml::array;
  using ::toml::parse_result;
}
