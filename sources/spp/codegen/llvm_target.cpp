module;
#include <spp/macros.hpp>

module spp.codegen.llvm_target;
import spp.utils.ice;
import spp.utils.types;
import llvm;
import std;

namespace spp::codegen {
  namespace {
    /// Whether the target has been read, and so fixed: selecting
    /// another after that would leave everything already built for
    /// the first.
    auto TargetFixed() -> bool& {
      static auto fixed = false;
      return fixed;
    }

    /// The triple this build emits for, written into by
    /// "SelectTarget". If it is empty, then it's the host, as
    /// "TargetTriple" specifies. Only written once, before
    /// codegen begins.
    auto SelectedTriple() -> Str& {
      // Static string returned as a mutable reference for
      // writing into.
      static auto triple = Str();
      return triple;
    }

    /// The triple the host llvm was configured for, normalised
    /// once. Read from llvm so that an arm64 or windows build
    /// asks its own backend for a layout and code generator
    /// instead of x86's.
    auto HostTriple() -> llvm::Triple const& {
      // Pull the triple from the llvm-known default, and run it
      // through normalization. Static because it's always going
      // to be the same.
      static const auto triple = llvm::Triple(
        llvm::Triple::normalize(llvm::sys::getDefaultTargetTriple()));
      return triple;
    }

    /// Register the backends that this build was linked against,
    /// once. The registry is what a triple is looked up in, so
    /// nothing can be said about a requested target until this
    /// has run.
    auto InitializeAllBackends()
      -> void {
      SPP_ATTR_MAYBE_UNUSED static const auto once = [] {
      #if defined(SPP_ALL_TARGETS)
        // All targets, including the host. This is used for
        // the CI pipeline cross-compilation checks, and for
        // normal cross-compilation.
        llvm::InitializeAllTargetInfos();
        llvm::InitializeAllTargets();
        llvm::InitializeAllTargetMCs();
        llvm::InitializeAllAsmPrinters();
      #else
        // Native targets only (normal compilation, default to
        // the host).
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
      #endif
        return true;
      }();
    }

    /// Canonical triples worth suggesting when a "--target" is
    /// not recognised. Not a whitelist, just examples.
    constexpr char const *kSuggestedTriples[] = {
      "x86_64-unknown-linux-gnu",
      "x86_64-unknown-linux-musl",
      "i686-unknown-linux-gnu",
      "aarch64-unknown-linux-gnu",
      "riscv64-unknown-linux-gnu",
      "s390x-unknown-linux-gnu",
      "powerpc64le-unknown-linux-gnu",
      "wasm32-unknown-wasip1",
    };

    /// Print the suggestions that this build can actually emit
    /// for. Nothing that shows in "--targets" fails the lookup
    /// check.
    auto PrintSupportedTriples() -> unsigned {
      InitializeAllBackends();
      auto printed = 0u;
      for (const auto candidate : kSuggestedTriples) {
        auto error = Str();
        const auto triple = llvm::Triple(llvm::Triple::normalize(candidate));
        if (llvm::TargetRegistry::lookupTarget(triple, error) == nullptr) { continue; }
        llvm::errs() << "  " << candidate << "\n";
        printed += 1;
      }
      return printed;
    }

    /// Get the target machine from the target triple. Every
    /// module is built against this target machine. Done at
    /// startup.
    auto SelectedTargetMachine() -> llvm::TargetMachine* {
      static const auto machine = []() -> llvm::TargetMachine* {
        // Initialize all the backends for the configuration.
        // This makes target machines available.
        InitializeAllBackends();

        // Get the target triple (what we are compiling for),
        // and lookup the target for it.
        auto error = Str();
        const auto &triple = TargetTriple();
        const auto *target = llvm::TargetRegistry::lookupTarget(triple, error);
        if (target == nullptr) {
          llvm::errs() << "No llvm target for " << triple.str() << ": " << error << "\n";
          return nullptr;
        }

        // The baseline the target's distributions build for, where
        // LLVM's generic CPU is less: on riscv64 Linux that is
        // rv64gc, with floating-point arguments in float registers
        // (lp64d), as its libc and every library are built.
        auto features = Str();
        auto options = llvm::TargetOptions();
        if (triple.isRISCV64() and triple.isOSLinux()) {
          features = "+m,+a,+f,+d,+c";
          options.MCOptions.ABIName = "lp64d";
        }

        // LLVM's "generic" CPU is each target's baseline, but on
        // sparc64 it is the 32-bit V8, which cannot load 64 bits:
        // there, V9, as LLVM itself picks when given no CPU.
        const auto cpu = triple.isSPARC64() ? "v9" : "generic";

        // "Position independent" because the runtime the program
        // links against is a shared library, and a non-pic object
        // cannot be linked against one on this target.
        return target->createTargetMachine(
          triple, cpu, features, options, llvm::Reloc::PIC_);
      }();
      return machine;
    }

    /// Create the folder for the triple: "<arch>-<os>[-<env>]",
    /// built from the parsed enums.
    auto FolderOf(
      llvm::Triple const &triple) -> Str {
      auto folder = llvm::Triple::getArchTypeName(triple.getArch()).str();
      folder += "-";
      folder += triple.isMacOSX()
        ? "macos"
        : llvm::Triple::getOSTypeName(triple.getOS()).str();
      if (triple.getEnvironment() != llvm::Triple::UnknownEnvironment) {
        folder += "-";
        folder += llvm::Triple::getEnvironmentTypeName(triple.getEnvironment()).str();
      }
      return folder;
    }

    /// Convert a folder name into a llvm triple (optional because
    /// it could fail).
    auto TripleOfFolder(
      Str const &folder) -> Opt<llvm::Triple> {
      auto parts = llvm::SmallVector<llvm::StringRef, 4>();
      llvm::StringRef(folder).split(parts, '-');
      if (parts.size() != 2 and parts.size() != 3) { return Nullopt; }
      return llvm::Triple(llvm::Triple::normalize(folder));
    }
  }
}

auto spp::codegen::TargetTriple() -> llvm::Triple const& {
  static const auto triple = [] {
    TargetFixed() = true;
    return SelectedTriple().empty() ? HostTriple() : llvm::Triple(llvm::Triple::normalize(SelectedTriple()));
  }();
  return triple;
}

auto spp::codegen::SelectTarget(
  char const *triple) -> bool {
  // An empty request is the host, which is what the target
  // resolves to on its own when nothing has been selected.
  const auto requested = Str(triple != nullptr ? triple : "");
  if (requested.empty()) { return true; }

  // Normalised before the lookup, so a short name ("riscv64",
  // "aarch64") is accepted on the same terms as a full triple.
  InitializeAllBackends();
  const auto normalized = llvm::Triple::normalize(requested);

  auto error = Str();
  if (llvm::TargetRegistry::lookupTarget(llvm::Triple(normalized), error) == nullptr) {
    llvm::errs()
      << "No llvm backend for target '" << requested << "' (normalised: " << normalized << "): " << error << "\n"
      << "This build can emit for:\n";

    // Triples rather than the registry's architecture names, which
    // is what the message used to print: "x86-64" is not something
    // that can be handed back to "--target", and a suggestion that
    // cannot be copied is not a suggestion.
    if (PrintSupportedTriples() == 0) {
      llvm::errs() << "  (nothing; this llvm has no usable backend at all)\n";
    }
    llvm::errs() << "Any other triple whose backend is linked in is accepted too; these are the tested ones.\n";
#if !defined(SPP_ALL_TARGETS)
    llvm::errs() << "Only the host backend is linked in; reconfigure with -DSPP_ALL_TARGETS=ON for the rest.\n";
#endif
    return false;
  }

  // Read before now (a static initialiser that asks for the
  // target, say), the host was fixed in its place.
  if (TargetFixed() and TargetTriple() != llvm::Triple(normalized)) {
    Ice("the target was read before '" + requested + "' was selected, and fixed to " + TargetTriple().str());
  }
  SelectedTriple() = normalized;
  return true;
}

/// [CHECKED]
auto spp::codegen::TargetIsHost() -> bool {
  // Compare the target and host triples; if they match then
  // the target is the host, and linking can proceed.
  return TargetTriple() == HostTriple();
}

/// [CHECKED]
auto spp::codegen::TargetIsLinux() -> bool {
  return TargetTriple().isOSLinux();
}

auto spp::codegen::TargetIsMusl() -> bool {
  return TargetTriple().isMusl();
}

auto spp::codegen::GnuTripleOf(
  const StrView folder) -> Str {
  const auto arch_end = folder.find('-');
  const auto arch = folder.substr(0, arch_end);
  const auto spelled = arch == "i386" ? StrView("i686") : arch == "sparcv9" ? StrView("sparc64") : arch;
  return Str(spelled) + Str(arch_end == StrView::npos ? StrView() : folder.substr(arch_end));
}

auto spp::codegen::HasSafeStack(
  const StrView triple) -> bool {
  const auto libcalls = llvm::RTLIB::RuntimeLibcallsInfo(llvm::Triple(llvm::StringRef(triple)));
  return libcalls.getSupportedLibcallImpl("__safestack_unsafe_stack_ptr") != llvm::RTLIB::Unsupported
    or libcalls.getSupportedLibcallImpl("__safestack_pointer_address") != llvm::RTLIB::Unsupported;
}

auto spp::codegen::TargetHasSafeStack() -> bool {
  return HasSafeStack(TargetTriple().str());
}

auto spp::codegen::TargetGnuTriple() -> char const* {
  static const auto gnu = GnuTripleOf(FolderOf(TargetTriple()));
  return gnu.c_str();
}

auto spp::codegen::TargetPointerBits() -> std::uint32_t {
  // From the data layout, not the architecture: x32 and its kind
  // are 64-bit architectures with 32-bit pointers.
  static const auto bits = [] {
    auto const *const machine = SelectedTargetMachine();
    SPP_ASSERT(machine != nullptr);
    return machine->createDataLayout().getPointerSizeInBits(0);
  }();
  return bits;
}

auto spp::codegen::TargetSizeType(
  llvm::LLVMContext &context) -> llvm::IntegerType* {
  return llvm::Type::getIntNTy(context, TargetPointerBits());
}

auto spp::codegen::TargetCfgValue(
  const StrView key) -> Str {
  auto const &triple = TargetTriple();
  if (key == "arch") { return llvm::Triple::getArchTypeName(triple.getArch()).str(); }
  if (key == "os") { return triple.isMacOSX() ? Str("macos") : llvm::Triple::getOSTypeName(triple.getOS()).str(); }
  if (key == "env") {
    return triple.getEnvironment() == llvm::Triple::UnknownEnvironment
      ? Str()
      : llvm::Triple::getEnvironmentTypeName(triple.getEnvironment()).str();
  }
  if (key == "family") { return triple.isOSWindows() ? "windows" : triple.isWasm() ? "wasm" : "unix"; }
  if (key == "pointer_width") { return std::to_string(TargetPointerBits()); }
  if (key == "endian") { return triple.isLittleEndian() ? "little" : "big"; }
  return Str();
}

auto spp::codegen::IsCfgValueValid(
  const StrView key, const StrView value) -> bool {
  // The names LLVM gives the architectures, systems and
  // environments it knows, as the folder names spell them.
  if (key == "arch") {
    for (auto a = 1; a <= llvm::Triple::LastArchType; ++a) {
      if (llvm::Triple::getArchTypeName(
        static_cast<llvm::Triple::ArchType>(a)) == llvm::StringRef(value)) { return true; }
    }
    return false;
  }
  if (key == "os") {
    if (value == "macos") { return true; }
    for (auto o = 1; o <= llvm::Triple::LastOSType; ++o) {
      if (llvm::Triple::getOSTypeName(
        static_cast<llvm::Triple::OSType>(o)) == llvm::StringRef(value)) { return true; }
    }
    return false;
  }
  if (key == "env") {
    for (auto e = 1; e <= llvm::Triple::LastEnvironmentType; ++e) {
      if (llvm::Triple::getEnvironmentTypeName(
        static_cast<llvm::Triple::EnvironmentType>(e)) == llvm::StringRef(value)) { return true; }
    }
    return false;
  }
  if (key == "family") { return value == "unix" or value == "windows" or value == "wasm"; }
  if (key == "pointer_width") { return value == "16" or value == "32" or value == "64"; }
  if (key == "endian") { return value == "little" or value == "big"; }
  return false;
}

/// [CHECKED]
auto spp::codegen::TargetFolderName() -> char const* {
  // The target folder name is the "folder of" the target triple,
  // ie the target that we are compiling to.
  static const auto folder = FolderOf(TargetTriple());
  return folder.c_str();
}

/// [CHECKED]
auto spp::codegen::HostFolderName() -> char const* {
  // The host folder name is the "folder of" the host triple, ie
  // the target that we are compiling to.
  static const auto folder = FolderOf(HostTriple());
  return folder.c_str();
}

/// [CHECKED]
auto spp::codegen::IsTargetFolderName(
  char const *folder) -> bool {
  const auto spelled = Str(folder);
  const auto triple = TripleOfFolder(spelled);
  if (not triple.has_value()) { return false; }
  if (triple->getArch() == llvm::Triple::UnknownArch) { return false; }
  if (triple->getOS() == llvm::Triple::UnknownOS) { return false; }
  return FolderOf(*triple) == spelled;
}

/// [CHECKED]
auto spp::codegen::SharedLibraryExtension(
  char const *folder) -> char const* {
  const auto triple = TripleOfFolder(folder);
  SPP_ASSERT(triple.has_value());
  if (triple->isOSWindows()) { return "dll"; }
  if (triple->isOSDarwin()) { return "dylib"; }
  return "so";
}

/// [CHECKED]
auto spp::codegen::ApplyTargetToModule(
  void *llvm_module) -> void {
  // Both off the same machine, rather than off the selected triple
  // directly: a backend is allowed to answer with a triple other
  // than the one it was looked up by, and taking both from the
  // machine is what keeps a module's triple and layout describing
  // the same target.
  const auto *machine = SelectedTargetMachine();
  if (machine == nullptr) { return; }

  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  llvm_mod.setTargetTriple(machine->getTargetTriple());
  llvm_mod.setDataLayout(machine->createDataLayout());

  // The machine's PIC relocation model is not enough on its own:
  // a module with no PIC level counts as non-PIC, so an external
  // global (AArch64's "__stack_chk_guard") is read directly, which
  // a PIE link rejects. The PIE level keeps the module's own
  // symbols direct; COFF has no PIE, as with clang.
  llvm_mod.setPICLevel(llvm::PICLevel::BigPIC);
  if (not machine->getTargetTriple().isOSBinFormatCOFF()) {
    llvm_mod.setPIELevel(llvm::PIELevel::Large);
  }
}

auto spp::codegen::AssertIntrinsicNamingIsSound() -> void {
  // Once per process, and not a walk of anything: this asks
  // llvm to mangle one intrinsic name and checks the answer,
  // because the failure being guarded against is not in any
  // particular module - it is that "Intrinsic::getName" itself
  // comes back wrong, which makes every overloaded intrinsic
  // in every module unnameable at once.
  SPP_ATTR_MAYBE_UNUSED static const auto checked = [] {
    auto ctx = llvm::LLVMContext();
    auto scratch = llvm::Module("spp.intrinsic.naming.check", ctx);
    auto *const i32 = llvm::Type::getInt32Ty(ctx);
    auto *const i1 = llvm::Type::getInt1Ty(ctx);
    auto *const fn_ty = llvm::FunctionType::get(llvm::StructType::get(ctx, {i32, i1}), {i32, i32}, false);

    constexpr auto expected = llvm::StringRef("llvm.sadd.with.overflow.i32");
    const auto actual = llvm::Intrinsic::getName(llvm::Intrinsic::sadd_with_overflow, {i32}, &scratch, fn_ty);
    if (actual == expected) { return true; }

    llvm::errs()
      << "spp: llvm is mangling intrinsic names wrongly - got '" << actual << "', expected '" << expected << "'.\n"
      << "     Every overloaded intrinsic will now fail module verification. This is the libstdc++ '_M_create'\n"
      << "     incompatibility (GCC bug 127228), which '-fvisibility-inlines-hidden' in CMakeLists.txt keeps this\n"
      << "     compiler clear of; check that flag still reaches every translation unit.\n";
    return false;
  }();

  SPP_ASSERT(checked);
}

auto spp::codegen::EmitObjectFile(
  void *llvm_module, char const *path) -> bool {
  auto &llvm_mod = *static_cast<llvm::Module*>(llvm_module);
  auto *machine = SelectedTargetMachine();
  if (machine == nullptr) { return false; }

  // Check we can open the given path.
  auto ec = std::error_code();
  auto out = llvm::raw_fd_ostream(path, ec, llvm::sys::fs::OF_None);
  if (ec) {
    llvm::errs() << "Could not open " << path << ": " << ec.message() << "\n";
    return false;
  }

  // The legacy pass manager, because that is the only one
  // the code generator is driven through.
  auto codegen_pm = llvm::legacy::PassManager();
  if (machine->addPassesToEmitFile(codegen_pm, out, nullptr, llvm::CodeGenFileType::ObjectFile)) {
    llvm::errs() << "The target cannot emit an object file\n";
    return false;
  }

  codegen_pm.run(llvm_mod);
  out.flush();

  // LLVM reports what it cannot do (an unsupported feature on the
  // target, say) as an error diagnostic, printed above, and then
  // carries on: the object it writes is not to be linked.
  if (llvm_mod.getContext().getDiagHandlerPtr()->HasErrors) {
    out.close();
    std::filesystem::remove(path);
    llvm::errs() << "LLVM reported errors building " << path << ", so no object was written\n";
    return false;
  }
  return true;
}
