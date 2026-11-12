module;
#include <spp/macros.hpp>

module spp.compiler.out_layout;
import spp.codegen.llvm_target;
import spp.utils.files;
import genex;
import magic_enum;
import std;

namespace spp::compiler {
  /// The name of an enum value in lower case: how it is written by the user.
  template <typename E>
  static auto LowerName(const E value) -> Str;

  /// The lower case name of every value of an enum, in declaration order.
  template <typename E>
  static auto LowerNames() -> Vec<Str>;

  /// The value of an enum that a name written by the user means, if any.
  template <typename E>
  static auto FromName(const StrView name) -> Opt<E>;
}

template <typename E>
auto spp::compiler::LowerName(const E value) -> Str {
  return magic_enum::enum_name(value)
    | genex::strings::lower_case
    | genex::to<Str>();
}

template <typename E>
auto spp::compiler::LowerNames() -> Vec<Str> {
  auto names = Vec<Str>();
  for (const auto value : magic_enum::enum_values<E>()) {
    names.emplace_back(LowerName(value));
  }
  return names;
}

template <typename E>
auto spp::compiler::FromName(const StrView name) -> Opt<E> {
  return magic_enum::enum_cast<E>(name, magic_enum::case_insensitive);
}

auto spp::compiler::ModeName(
  const BuildMode mode) -> Str {
  return LowerName(mode);
}

auto spp::compiler::ModeNames() -> Vec<Str> {
  return LowerNames<BuildMode>();
}

auto spp::compiler::ModeFromName(
  const StrView name) -> Opt<BuildMode> {
  return FromName<BuildMode>(name);
}

auto spp::compiler::TypeName(
  const BuildType type) -> Str {
  return LowerName(type);
}

auto spp::compiler::TypeNames() -> Vec<Str> {
  return LowerNames<BuildType>();
}

auto spp::compiler::TypeFromName(
  const StrView name) -> Opt<BuildType> {
  return FromName<BuildType>(name);
}

SPP_MOD_BEGIN
/// [CHECKED]
auto spp::compiler::OutLayout::AllTargetsRoot() const -> Path {
  return Root / "out";
}

/// [CHECKED]
auto spp::compiler::OutLayout::OutRoot() const -> Path {
  return AllTargetsRoot() / Target / ModeName(Mode);
}

/// [CHECKED]
auto spp::compiler::OutLayout::LlvmRoot() const -> Path {
  return OutRoot() / "llvm";
}

/// [CHECKED]
auto spp::compiler::OutLayout::LibRoot() const -> Path {
  return OutRoot() / "lib";
}

/// [CHECKED]
auto spp::compiler::OutLayout::LtoIrFile() const -> Path {
  return LlvmRoot() / "lto.ll";
}

/// [CHECKED]
auto spp::compiler::OutLayout::ObjectFile() const -> Path {
  return LlvmRoot() / "spp.o";
}

auto spp::compiler::OutLayout::LineTableFile() const -> Path {
  return LlvmRoot() / "spp-lines.o";
}

/// [CHECKED]
auto spp::compiler::OutLayout::ExecutablePath() const -> Path {
  return OutRoot() / ExecutableName();
}

/// [CHECKED]
auto spp::compiler::OutLayout::ExecutableName() const -> Str {
  return utils::files::NativeString(Root.filename())
    + codegen::ExecutableSuffix(Target.c_str());
}

SPP_MOD_END
