module;
#include <spp/macros.hpp>

module spp.compiler.out_layout;
import spp.utils.files;
import std;

SPP_MOD_BEGIN
auto spp::compiler::OutLayout::AllTargetsRoot() const
  -> std::filesystem::path {
  return Root / "out";
}

auto spp::compiler::OutLayout::OutRoot() const
  -> std::filesystem::path {
  return AllTargetsRoot() / Target / Mode;
}

auto spp::compiler::OutLayout::LlvmRoot() const
  -> std::filesystem::path {
  return OutRoot() / "llvm";
}

auto spp::compiler::OutLayout::LibRoot() const
  -> std::filesystem::path {
  return OutRoot() / "lib";
}

auto spp::compiler::OutLayout::LtoIrFile() const
  -> std::filesystem::path {
  return LlvmRoot() / "lto.ll";
}

auto spp::compiler::OutLayout::ObjectFile() const
  -> std::filesystem::path {
  return LlvmRoot() / "spp.o";
}

auto spp::compiler::OutLayout::ExecutablePath() const
  -> std::filesystem::path {
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
