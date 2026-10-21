module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_target;
import llvm;

namespace spp::codegen {
  /// The triple every module is built for, normalised once:
  /// the selected target, or the host when none was asked for.
  /// Being different from the host is a "cross compilation".
  /// For the code generation that decides by target.
  SPP_EXP_FUN auto TargetTriple() -> llvm::Triple const&;

  /// Choose the triple every module is built for. Must be
  /// called before any code generation: the target machine,
  /// the triple and the data layout are each resolved once, on
  /// first use, and a selection made after that is ignored
  /// rather than honoured - which would leave modules
  /// describing one target and the object describing another.
  ///
  /// The triple is normalised, so a short name ("riscv64",
  /// "aarch64") is accepted like a full triple; null or empty
  /// means the host. A target whose backend this llvm was not
  /// built with is rejected here, listing the ones it does
  /// have, rather than when an object cannot be emitted for
  /// it. Answers whether the target was registered and
  /// selected.
  SPP_EXP_FUN auto SelectTarget(char const *triple) -> bool;

  /// Whether the selected target is the machine this compiler
  /// is running on. Only a host build can be linked and run:
  /// the linker driver invoked below is the host's, and the
  /// ffi runtimes a project ships are host objects.
  SPP_EXP_FUN auto TargetIsHost() -> bool;

  /// Whether the selected target's operating system is Linux,
  /// for the measures only a Linux kernel has.
  SPP_EXP_FUN auto TargetIsLinux() -> bool;

  /// The selected target as a folder name: "<arch>-<os>[-<env>]",
  /// such as "x86_64-linux-gnu", "aarch64-macos" or
  /// "x86_64-windows-msvc". It names the target's folder in the
  /// "out" tree, and the folder an ffi package ships its library
  /// for the target in. One spelling per target, however the
  /// target was asked for. Fixes the target to the host if read
  /// before "SelectTarget".
  SPP_EXP_FUN auto TargetFolderName() -> char const*;

  /// The host's folder name, in the same spelling. Safe to read
  /// before "SelectTarget", which "TargetFolderName" is not.
  SPP_EXP_FUN auto HostFolderName() -> char const*;

  /// Whether "folder" is the canonical folder name of some
  /// target with a known architecture and operating system.
  /// Another spelling of a known target ("arm64-linux-gnu",
  /// "x86_64-darwin") is not, because the compiler would never
  /// look in it.
  SPP_EXP_FUN auto IsTargetFolderName(char const *folder) -> bool;

  /// The extension a shared library carries on the target a
  /// folder names, without the leading dot: "so", "dylib" or
  /// "dll". "folder" must pass "IsTargetFolderName".
  SPP_EXP_FUN auto SharedLibraryExtension(char const *folder) -> char const*;

  /// Stamp an "llvm::Module" (passed as an opaque pointer)
  /// with the selected target's triple and data layout.
  SPP_EXP_FUN auto ApplyTargetToModule(void *llvm_module) -> void;

  /// Emit an "llvm::Module" (passed as an opaque pointer) as a
  /// native object file at "path". Answers whether the object
  /// file was written.
  SPP_EXP_FUN auto EmitObjectFile(void *llvm_module, char const *path) -> bool;

  /// Check that llvm can still spell an overloaded intrinsic's
  /// name, and stop if it cannot.
  ///
  /// The names come from "Intrinsic::getName" as a
  /// "std::string", and a libstdc++ incompatibility (GCC bug
  /// 127228) makes that come back padded with uninitialised
  /// bytes; "-fvisibility-inlines-hidden" (CMakeLists.txt)
  /// keeps this compiler clear of it. While it bites, no
  /// overloaded intrinsic can be named correctly and every
  /// module holding one is rejected - by the verifier, whose
  /// complaint is about the name and says nothing about why it
  /// is wrong. This asks for one name and compares it, so the
  /// failure is reported where the cause is rather than
  /// several layers downstream.
  ///
  /// Runs its check once per process, so it costs nothing to
  /// call wherever it is convenient. The diagnosis is printed,
  /// and the build stopped, in any build.
  SPP_EXP_FUN auto AssertIntrinsicNamingIsSound() -> void;
}
