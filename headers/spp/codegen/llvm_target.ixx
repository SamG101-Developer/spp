module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_target;
import spp.utils.types;
import llvm;
import std;

namespace spp::codegen {
  /// The target triple is the triple for the platform that s++ is compiling
  /// for. It gets normalized, and cones from the "selected target". If it
  /// doesn't match the host triple, then we are cross compiling.
  SPP_EXP_FUN auto TargetTriple() -> llvm::Triple const&;

  /// Choose the triple for the build. This is checked against a currently
  /// selected triple, if present, otherwise this triple becomes the master
  /// check. Normalized for short names too.
  SPP_EXP_FUN auto SelectTarget(char const *triple) -> bool;

  /// Choose the CPU the program is built for ("[target.floor] cpu"):
  /// "generic", the target's baseline (the default), "native", this
  /// machine's CPU when building for it, or an LLVM CPU name for the
  /// target ("x86-64-v3", "apple-m1"). Must come before anything builds
  /// the target machine. Answers false, having said why, for a CPU the
  /// target does not have, or "native" when cross compiling.
  SPP_EXP_FUN auto SelectCpu(char const *cpu) -> bool;

  /// Whether the selected target is the machine this compiler
  /// is running on. Only a host build can be run here; another
  /// Linux target is linked with its cross linker.
  SPP_EXP_FUN auto TargetIsHost() -> bool;

  /// Whether the selected target's operating system is Linux,
  /// for the measures only a Linux kernel has.
  SPP_EXP_FUN auto TargetIsLinux() -> bool;

  /// Whether the selected target's C library is musl, whose
  /// libraries have no fortified (glibc "_chk") calls to check.
  SPP_EXP_FUN auto TargetIsMusl() -> bool;

  /// Whether the selected target's code is position independent, and so its
  /// executables PIE. Every target but m68k, whose backend reaches calls and
  /// the GOT from PIC code through 16-bit offsets, so past about 32 KB of code
  /// the program cannot link.
  SPP_EXP_FUN auto TargetIsPositionIndependent() -> bool;

  /// A target folder as a GNU toolchain names the target, the
  /// prefix of its cross tools ("s390x-linux-gnu" for
  /// "s390x-linux-gnu-gcc"): the folder name, but for the
  /// architectures GNU spells otherwise (i386 as i686, sparcv9 as
  /// sparc64).
  SPP_EXP_FUN auto GnuTripleOf(StrView folder) -> Str;

  /// The selected target's "GnuTripleOf".
  SPP_EXP_FUN auto TargetGnuTriple() -> char const*;

  /// The width of a pointer on the selected target, in bits: what
  /// "USize" is, through "!cfg(pointer_width=...)".
  SPP_EXP_FUN auto TargetPointerBits() -> std::uint32_t;

  /// The integer type as wide as a pointer on the selected target:
  /// what a "USize" lowers to, and a "size_t" is.
  SPP_EXP_FUN auto TargetSizeType(llvm::LLVMContext &context) -> llvm::IntegerType*;

  /// The selected target's value for a "!cfg" key: "os", "arch",
  /// "env", "family", "pointer_width" or "endian", spelled as its
  /// folder name spells it ("macos", "sparcv9", "gnueabihf").
  /// Empty where the target has none, as for an unknown env.
  SPP_EXP_FUN auto TargetCfgValue(StrView key) -> Str;

  /// Whether "value" is one a "!cfg" key can have on some target,
  /// so that a condition naming any other is a mistake rather than
  /// one that never holds.
  SPP_EXP_FUN auto IsCfgValueValid(StrView key, StrView value) -> bool;

  /// The selected target as a folder name, formatted as "<arch>-<os>[-<env>]",
  /// ie "x86_64-linux-gnu", "aarch64-macos" or "x64_64-windows-msvc". This is
  /// the target's folder in the "out" directory.
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

  /// The extension of the import library a shared library is
  /// linked through on the target a folder names, without the
  /// leading dot: "lib" on an MSVC target, which links
  /// "sppc.lib" and loads "sppc.dll". Null where the shared
  /// library itself is linked. "folder" must pass
  /// "IsTargetFolderName".
  SPP_EXP_FUN auto ImportLibraryExtension(char const *folder) -> char const*;

  /// What an executable's name ends in on the target a folder
  /// names: ".exe" on Windows, nothing elsewhere. Nothing for a
  /// folder that names no target.
  SPP_EXP_FUN auto ExecutableSuffix(char const *folder) -> char const*;

  /// Stamp an "llvm::Module" with the selected target's triple
  /// and data layout.
  SPP_EXP_FUN auto ApplyTargetToModule(llvm::Module &llvm_mod) -> void;

  /// Give every function defined in "llvm_mod" the selected CPU and
  /// its features, as the "target-cpu" and "target-features" LLVM
  /// reads each function's target from. A function's own features
  /// are kept, after the program's, so they win: where a function
  /// built for a wider CPU than the program's would say so.
  SPP_EXP_FUN auto ApplyTargetToFunctions(llvm::Module &llvm_mod) -> void;

  /// The selected target's machine, built once on first use;
  /// every module is built against it. Null when no backend
  /// serves the selected triple.
  SPP_EXP_FUN auto SelectedTargetMachine() -> llvm::TargetMachine*;

  /// Emit an "llvm::Module" as a native object file at "path".
  /// Answers whether the object file was written. "opt_level" (0
  /// to 3, as the optimisation pipeline takes it) sets how hard
  /// the backend works: instruction selection and register
  /// allocation.
  SPP_EXP_FUN auto EmitObjectFile(llvm::Module &llvm_mod, char const *path, unsigned opt_level) -> bool;

  /// Check that all the intrinsic names are sound, ie spelled correctly and
  /// non-corrupted. This is because of some LLVM and GCC bugs causing mangled
  /// intrinsic names. GCC bug 127228 has been patched, keeping as failsafe.
  SPP_EXP_FUN auto AssertIntrinsicNamingIsSound() -> void;
}
