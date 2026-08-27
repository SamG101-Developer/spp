module;
#include <CLI/CLI.hpp>

export module cli11;

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
