// The one TU that defines toml++'s non-template functions;
// see headers/wrappers/tomlpp.ixx. A plain TU, not a module
// unit, so the bodies are compiled against the textual std
// headers they were written for.
#define TOML_HEADER_ONLY 0
#define TOML_IMPLEMENTATION
#include <toml++/toml.hpp>
