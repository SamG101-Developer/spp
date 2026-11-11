module;
#include <spp/macros.hpp>

module spp.codegen.llvm_support;
import llvm;
import std;

auto spp::codegen::DoesTargetSupportSafeStack(
  llvm::Triple const &triple) -> bool {
  // sppc is a DLL on Windows, which cannot export the thread-local pointer.
  if (triple.isOSWindows()) { return false; }
  const auto libcalls = llvm::RTLIB::RuntimeLibcallsInfo(triple);
  return libcalls.getSupportedLibcallImpl("__safestack_unsafe_stack_ptr") != llvm::RTLIB::Unsupported
    or libcalls.getSupportedLibcallImpl("__safestack_pointer_address") != llvm::RTLIB::Unsupported;
}

auto spp::codegen::DoesTargetSupportSafeStackCanary(
  llvm::Triple const &triple) -> bool {
  const auto libcalls = llvm::RTLIB::RuntimeLibcallsInfo(triple);
  return libcalls.getSupportedLibcallImpl("__stack_chk_fail") != llvm::RTLIB::Unsupported;
}

auto spp::codegen::DoesTargetSupportInlineStackProbes(
  llvm::Triple const &triple) -> bool {
  return not triple.isOSWindows();
}

auto spp::codegen::DoesTargetSupportSlsHardening(
  llvm::Triple const &triple) -> bool {
  return triple.isX86() or (triple.isAArch64() and not triple.isOSBinFormatMachO());
}

auto spp::codegen::DoesTargetSupportRegisterScrubbing(
  llvm::Triple const &triple) -> bool {
  return triple.isX86() or triple.isAArch64();
}

auto spp::codegen::DoesTargetSupportPcRelEhPointers(
  llvm::Triple const &triple) -> bool {
  return triple.getArch() != llvm::Triple::m68k;
}

auto spp::codegen::DoesTargetSupportKcfi(
  llvm::Triple const &triple) -> bool {
  return triple.getArch() == llvm::Triple::x86_64 or triple.isAArch64();
}

auto spp::codegen::DoesTargetSupportBti(
  llvm::Triple const &triple) -> bool {
  return triple.isAArch64() and triple.isOSBinFormatELF();
}

auto spp::codegen::DoesTargetSupportPacRet(
  llvm::Triple const &triple) -> bool {
  return DoesTargetSupportBti(triple);
}

auto spp::codegen::DoesTargetSupportIndirectBranches(
  llvm::Triple const &triple) -> bool {
  return not triple.isWasm();
}
