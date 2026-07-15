module;
#include <toml++/toml.hpp>

export module tomlpp;

export namespace toml {
    using ::toml::parse_file;
}
