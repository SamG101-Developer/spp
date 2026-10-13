module;
#include <spp/macros.hpp>

module spp.compiler.out_layout;
import spp.utils.files;
import std;

SPP_MOD_BEGIN
auto spp::compiler::OutLayout::AllTargetsRoot() const
  -> Path {
  return Root / "out";
}

auto spp::compiler::OutLayout::OutRoot() const
  -> Path {
  return AllTargetsRoot() / Target / Mode;
}

auto spp::compiler::OutLayout::LlvmRoot() const
  -> Path {
  return OutRoot() / "llvm";
}

auto spp::compiler::OutLayout::LibRoot() const
  -> Path {
  return OutRoot() / "lib";
}

auto spp::compiler::OutLayout::LtoIrFile() const
  -> Path {
  return LlvmRoot() / "lto.ll";
}

auto spp::compiler::OutLayout::ObjectFile() const
  -> Path {
  return LlvmRoot() / "spp.o";
}

auto spp::compiler::OutLayout::ExecutablePath() const
  -> Path {
  return OutRoot() / ExecutableName();
}

auto spp::compiler::OutLayout::ExecutableName() const
  -> Str {
  return utils::files::NativeString(Root.filename());
}

auto spp::compiler::OutLayout::AllModes()
  -> Vec<Str> {
  return Vec<Str>{"dev", "rel"};
}

SPP_MOD_END
