module;
#define CLI11_COMPILE
#include <CLI/CLI.hpp>

/// The CLI11 module is used for argument parsing to form the spp tool. The
/// module name contains a postfix underscore soa s not to conflict with CLI11's
/// own module logic.
export module cli11_;

export namespace CLI {
  using ::CLI::App;
  using ::CLI::ConfigBase;
  using ::CLI::Formatter;
  using ::CLI::IsMember;
  using ::CLI::RuntimeError;
  using ::CLI::ParseError;
  using ::CLI::CallForHelp;
  using ::CLI::CallForAllHelp;
}
