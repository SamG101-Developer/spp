module;
#include <spp/macros-platforms.hpp>
#include <spp/macros.hpp>

module spp.utils.shell;
import std;

auto spp::utils::shell::Quote(const StrView arg) -> Str {
  auto out = Str();
#if SPP_PLATFORM_WINDOWS
  // Backslashes are only special where they lead up to a double quote, so those
  // are doubled, including the ones before the closing quote.
  out += '"';
  auto slashes = 0uz;
  for (const auto c : arg) {
    if (c == '\\') {
      ++slashes;
      continue;
    }
    out.append(c == '"' ? slashes * 2 + 1 : slashes, '\\');
    out += c;
    slashes = 0;
  }
  out.append(slashes * 2, '\\');
  out += '"';
#else
  // Nothing is special inside single quotes, except a single quote, which has
  // to step outside them.
  out += '\'';
  for (const auto c : arg) {
    if (c == '\'') { out += "'\\''"; }
    else { out += c; }
  }
  out += '\'';
#endif
  return out;
}

auto spp::utils::shell::Run(Str const &command) -> int {
  // "cmd /c" takes a leading quote and the last quote off a line that starts
  // with one, unless the line is simple enough, so such a line is given an
  // outer pair for it to take.
#if SPP_PLATFORM_WINDOWS
  if (command.starts_with('"')) {
    return std::system(("\"" + command + "\"").c_str());
  }
#endif
  return std::system(command.c_str());
}
