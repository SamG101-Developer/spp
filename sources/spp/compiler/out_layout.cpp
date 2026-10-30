module;
#include <spp/macros.hpp>

module spp.compiler.out_layout;
import spp.utils.files;
import std;

SPP_MOD_BEGIN
/// [CHECKED]
auto spp::compiler::OutLayout::AllTargetsRoot() const -> Path {
  return Root / "out";
}

/// [CHECKED]
auto spp::compiler::OutLayout::OutRoot() const -> Path {
  return AllTargetsRoot() / Target / Mode;
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

/// [CHECKED]
auto spp::compiler::OutLayout::ExecutablePath() const -> Path {
  return OutRoot() / ExecutableName();
}

/// [CHECKED]
auto spp::compiler::OutLayout::ExecutableName() const -> Str {
  return utils::files::NativeString(Root.filename());
}

/// [CHECKED]
auto spp::compiler::OutLayout::AllModes() -> Vec<Str> {
  return Vec<Str>{"dev", "rel"};
}

SPP_MOD_END
