// The one TU that defines CLI11's non-template functions; see headers/wrappers/cli11.ixx. A plain TU, not a module
// unit, so the bodies are compiled against the textual std headers they were written for.
#define CLI11_COMPILE
#include <CLI/CLI.hpp>
#include <CLI/impl/App_inl.hpp>
#include <CLI/impl/Argv_inl.hpp>
#include <CLI/impl/Config_inl.hpp>
#include <CLI/impl/Encoding_inl.hpp>
#include <CLI/impl/ExtraValidators_inl.hpp>
#include <CLI/impl/Formatter_inl.hpp>
#include <CLI/impl/Option_inl.hpp>
#include <CLI/impl/Split_inl.hpp>
#include <CLI/impl/StringTools_inl.hpp>
#include <CLI/impl/TypeTools_inl.hpp>
#include <CLI/impl/Validators_inl.hpp>
