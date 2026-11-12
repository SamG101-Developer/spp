module;
#include <spp/macros.hpp>

module spp.codegen.llvm_target;
import spp.utils.ice;
import spp.utils.types;
import llvm;
import std;

namespace spp::codegen {
  namespace {
    /// [CHECKED]
    /// Whether the target has been read, and so fixed: selecting another after
    /// that would leave everything already built for the first.
    auto IsTargetFixed() -> bool& {
      // Static bool returned as a mutable reference for writing into.
      static auto fixed = false;
      return fixed;
    }

    /// [CHECKED]
    /// The triple this build emits for, written into by "SelectTarget". If it
    /// is empty, then it's the host, as "TargetTriple" specifies. Only written
    /// once, before codegen begins.
    auto SelectedTriple() -> Str& {
      // Static string returned as a mutable reference for writing into.
      static auto triple = Str();
      return triple;
    }

    /// The CPU chosen by "SelectCpu": "generic", "native" or an LLVM
    /// CPU name.
    auto SelectedCpu() -> Str& {
      static auto cpu = Str("generic");
      return cpu;
    }

    /// Whether the target machine has been built, after which the CPU
    /// can no longer change.
    auto IsMachineBuilt() -> bool& {
      static auto built = false;
      return built;
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

    /// [CHECKED]
    /// Register the backends that this build was linked against. The registry
    /// is what a triple is looked up in, so nothing can be said about a
    /// requested target until this has run.
    auto InitializeAllBackends() -> void {
      SPP_ATTR_MAYBE_UNUSED static const auto once = [] {
      #if defined(SPP_ALL_TARGETS)
        // All targets, including the host. This is used for the CI pipeline
        // cross-compilation checks, and for normal cross-compilation.
        llvm::InitializeAllTargetInfos();
        llvm::InitializeAllTargets();
        llvm::InitializeAllTargetMCs();
        llvm::InitializeAllAsmPrinters();
      #else
        // Native targets only (normal compilation, default to the host). Linux
        // for Linux, Windows for Windows etc.
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
      #endif
        return true;
      }();
    }

    /// Canonical triples worth suggesting when a "--target" is
    /// not recognised: the host lanes and the cross matrix in
    /// "_cross_codegen.yaml" test these. Not a whitelist.
    constexpr char const *kSuggestedTriples[] = {
      "x86_64-unknown-linux-gnu",
      "aarch64-apple-darwin",
      "x86_64-pc-windows-msvc",
      "i686-unknown-linux-gnu",
      "s390x-unknown-linux-gnu",
      "riscv64-unknown-linux-gnu",
      "aarch64-unknown-linux-gnu",
      "x86_64-unknown-linux-musl",
      "x86_64-unknown-linux-gnux32",
      "aarch64_be-unknown-linux-gnu",
      "armv7-unknown-linux-gnueabihf",
      "arm-unknown-linux-gnueabi",
      "powerpc64le-unknown-linux-gnu",
      "powerpc64-unknown-linux-gnu",
      "powerpc-unknown-linux-gnu",
      "mips-unknown-linux-gnu",
      "mipsel-unknown-linux-gnu",
      "mips64el-unknown-linux-gnuabi64",
      "sparc64-unknown-linux-gnu",
      "loongarch64-unknown-linux-gnu",
      "m68k-unknown-linux-gnu",
      "x86_64-pc-windows-gnu",
      "wasm32-unknown-wasip1",
      "x86_64-unknown-freebsd",
      "x86_64-apple-darwin",
    };

    /// [CHECKED]
    /// Print the suggestions that this build can actually emit for. Nothing
    /// that shows in "--targets" fails the lookup check.
    auto PrintSupportedTriples() -> unsigned {
      // Make sure that the backends are all initialized before trying any llvm
      // operations.
      InitializeAllBackends();
      auto printed = 0u;
      for (const auto candidate : kSuggestedTriples) {
        auto error = Str();

        // Normalize the candidate and create a triple for it. Look it up as a
        // target and check if it exists or not. Skip if it doesn't exist.
        const auto normalized = llvm::Triple::normalize(candidate);
        const auto triple = llvm::Triple(normalized);
        const auto target = llvm::TargetRegistry::lookupTarget(triple, error);
        if (target == nullptr) { continue; }

        // Otherwise it can be selected, (but wasn't), so add it to the error
        // stream for reporting.
        llvm::errs() << "  " << candidate << "\n";
        printed += 1;
      }

      // Return the number of candidate triples that are supported for this
      // system / installation.
      return printed;
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

auto spp::codegen::SelectedTargetMachine() -> llvm::TargetMachine* {
  static const auto machine = []() -> llvm::TargetMachine* {
    // Initialize all the backends for the configuration.
    // This makes target machines available.
    IsMachineBuilt() = true;
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
    // (lp64d), as its libc and every library are built. On
    // 32-bit PowerPC Linux it is the secure PLT, as gcc is
    // configured: LLVM's default puts the PLT in a writable,
    // executable segment, which the W^X link check rejects.
    auto features = Str();
    auto options = llvm::TargetOptions();
    if (triple.isRISCV64() and triple.isOSLinux()) {
      features = "+m,+a,+f,+d,+c";
      options.MCOptions.ABIName = "lp64d";
    }
    else if (triple.getArch() == llvm::Triple::ppc and triple.isOSLinux()) {
      features = "+secure-plt";
    }

    // LLVM's "generic" CPU is each target's baseline, but on
    // sparc64 it is the 32-bit V8, which cannot load 64 bits:
    // there, V9, as LLVM itself picks when given no CPU. "native"
    // is this machine's CPU, with the features it reports, on top
    // of the baseline's; any other is a CPU name "SelectCpu" checked.
    auto cpu = Str(triple.isSPARC64() ? "v9" : "generic");
    if (SelectedCpu() == "native") {
      cpu = llvm::sys::getHostCPUName().str();
      for (auto const &feature : llvm::sys::getHostCPUFeatures()) {
        features += (features.empty() ? "" : ",") + Str(feature.getValue() ? "+" : "-") + feature.getKey().str();
      }
    }
    else if (SelectedCpu() != "generic") { cpu = SelectedCpu(); }

    // Fixed addresses need a code model that reaches the whole
    // program: the small one still uses 16-bit offsets.
    if (not TargetIsPositionIndependent()) {
      return target->createTargetMachine(
        triple, cpu, features, options, llvm::Reloc::Static, llvm::CodeModel::Large);
    }

    // "Position independent" because the program is linked as
    // a PIE, with the runtime as a shared library.
    return target->createTargetMachine(
      triple, cpu, features, options, llvm::Reloc::PIC_);
  }();
  return machine;
}

/// [CHECKED]
auto spp::codegen::TargetTriple() -> llvm::Triple const& {
  // Set the "target fixed" flag to true, because once this is called, we never
  // change targets. Read from the selected triple if present, otherwise host.
  static const auto triple = [] {
    IsTargetFixed() = true;
    return SelectedTriple().empty()
      ? HostTriple()
      : llvm::Triple(llvm::Triple::normalize(SelectedTriple()));
  }();
  return triple;
}

/// [CHECKED]
auto spp::codegen::SelectTarget(char const *triple) -> bool {
  // If the triple is empty. then the target platform is the host platform,
  // which is automatically acceptable, so return true early.
  const auto requested = Str(triple != nullptr ? triple : "");
  if (requested.empty()) { return true; }

  // Initialize the backends, and then normalize the triple, so that short
  // names, like "riscv64" and "aarch64" are accepted like a normal triple.
  InitializeAllBackends();
  const auto normalized = llvm::Triple::normalize(requested);
  const auto normalized_triple = llvm::Triple(normalized);

  // If the lookup for the normalized triple fails, display an error and then
  // print the supported triples based on the usable backends.
  auto error = Str();
  if (llvm::TargetRegistry::lookupTarget(normalized_triple, error) == nullptr) {
    llvm::errs()
      << "No llvm backend for target '" << requested << "' (normalised: "
      << normalized << "): " << error << "\n" << "This build can emit for:\n";

    if (PrintSupportedTriples() == 0) {
      llvm::errs() << "  (nothing; this llvm has no usable backend at all)\n";
    }

    llvm::errs()
      << "Any other triple whose backend is linked in is accepted too; "
      << "these are the tested ones.\n";

#if !defined(SPP_ALL_TARGETS)
    llvm::errs()
      << "Only the host backend is linked in; "
      << "reconfigure with -DSPP_ALL_TARGETS=ON for the rest.\n";
#endif
    return false;
  }

  // If the target triple has already been configured, and is different to this
  // requested triple, raise an ICE error. Otherwise, write it into the selected
  // triple.
  if (IsTargetFixed() and TargetTriple() != normalized_triple) {
    Ice(
      "the target was read before '" + requested +
      "' was selected, and fixed to '" + TargetTriple().str() + "'");
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

auto spp::codegen::TargetIsPositionIndependent() -> bool {
  return TargetTriple().getArch() != llvm::Triple::m68k;
}

auto spp::codegen::GnuTripleOf(
  const StrView folder) -> Str {
  const auto arch_end = folder.find('-');
  const auto arch = folder.substr(0, arch_end);
  const auto spelled = arch == "i386" ? StrView("i686") : arch == "sparcv9" ? StrView("sparc64") : arch;
  return Str(spelled) + Str(arch_end == StrView::npos ? StrView() : folder.substr(arch_end));
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
  // The target folder name is the "folder of" the target triple, ie the target
  // that we are compiling to.
  static const auto folder = FolderOf(TargetTriple());
  return folder.c_str();
}

/// [CHECKED]
auto spp::codegen::HostFolderName() -> char const* {
  // The host folder name is the "folder of" the host triple, ie the target that
  // we are compiling to.
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
auto spp::codegen::ImportLibraryExtension(
  char const *folder) -> char const* {
  const auto triple = TripleOfFolder(folder);
  SPP_ASSERT(triple.has_value());
  return triple->isWindowsMSVCEnvironment() ? "lib" : nullptr;
}

/// [CHECKED]
auto spp::codegen::ExecutableSuffix(
  char const *folder) -> char const* {
  const auto triple = TripleOfFolder(folder);
  return triple.has_value() and triple->isOSWindows() ? ".exe" : "";
}

/// [CHECKED]
auto spp::codegen::ApplyTargetToModule(
  llvm::Module &llvm_mod) -> void {
  // Both off the same machine, rather than off the selected triple
  // directly: a backend is allowed to answer with a triple other
  // than the one it was looked up by, and taking both from the
  // machine is what keeps a module's triple and layout describing
  // the same target.
  const auto *machine = SelectedTargetMachine();
  if (machine == nullptr) { return; }

  llvm_mod.setTargetTriple(machine->getTargetTriple());
  llvm_mod.setDataLayout(machine->createDataLayout());
  if (not TargetIsPositionIndependent()) { return; }

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

/// [CHECKED]
auto spp::codegen::AssertIntrinsicNamingIsSound() -> void {
  // Mangle one intrinsic name and check the result is not corrupted and is
  // exactly what is expected.
  SPP_ATTR_MAYBE_UNUSED static const auto checked = [] {
    auto ctx = llvm::LLVMContext();
    auto scratch = llvm::Module("spp.intrinsic.naming.check", ctx);
    auto i32 = llvm::Type::getInt32Ty(ctx);
    auto i1 = llvm::Type::getInt1Ty(ctx);
    const auto fn_ty = llvm::FunctionType::get(
      llvm::StructType::get(ctx, {i32, i1}), {i32, i32}, false);

    constexpr auto expected = llvm::StringRef("llvm.sadd.with.overflow.i32");
    const auto actual = llvm::Intrinsic::getName(
      llvm::Intrinsic::sadd_with_overflow, {i32}, &scratch, fn_ty);
    if (actual == expected) { return true; }

    llvm::errs()
      << "spp: llvm is mangling intrinsic names incorrectly - got '" << actual
      << "', expected '" << expected << "'.\n";
    return false;
  }();

  SPP_ASSERT(checked);
}

auto spp::codegen::SelectCpu(
  char const *cpu) -> bool {
  if (IsMachineBuilt()) { spp::Ice("the CPU was chosen after the target machine was built"); }
  const auto name = StrView(cpu);
  if (name == "generic") {
    SelectedCpu() = Str(name);
    return true;
  }

  // "native" is this machine's CPU, which says nothing about another
  // target's.
  if (name == "native") {
    if (not TargetIsHost()) {
      llvm::errs()
        << "'native' names this machine's CPU, so it cannot be used when building for another target.\n"
        << "  Set cpu under [target.floor] in spp.toml to one of " << TargetTriple().str() << "'s CPUs, or 'generic'.\n";
      return false;
    }
    SelectedCpu() = Str(name);
    return true;
  }

  // Any other must be a CPU the target knows. An unknown one would
  // otherwise only be warned about, and the baseline used instead.
  auto error = Str();
  const auto &triple = TargetTriple();
  InitializeAllBackends();
  const auto target = llvm::TargetRegistry::lookupTarget(triple, error);
  const auto info = target == nullptr
    ? Unique<llvm::MCSubtargetInfo>() : Unique<llvm::MCSubtargetInfo>(target->createMCSubtargetInfo(triple, name, ""));
  if (info == nullptr or not info->isCPUStringValid(name)) {
    llvm::errs()
      << "'" << name << "' is not a CPU of " << triple.str() << ".\n"
      << "  Set cpu under [target.floor] in spp.toml to 'generic', 'native', or one listed by "
      << "'llc -mtriple=" << triple.str() << " -mcpu=help'.\n";
    return false;
  }
  SelectedCpu() = Str(name);
  return true;
}

auto spp::codegen::ApplyTargetToFunctions(
  llvm::Module &llvm_mod) -> void {
  const auto machine = SelectedTargetMachine();
  if (machine == nullptr) { return; }
  const auto cpu = machine->getTargetCPU();
  const auto features = machine->getTargetFeatureString();
  for (auto &fn : llvm_mod) {
    if (fn.isDeclaration()) { continue; }
    if (not fn.hasFnAttribute("target-cpu")) { fn.addFnAttr("target-cpu", cpu); }
    const auto own = fn.getFnAttribute("target-features").getValueAsString();
    if (not features.empty()) {
      fn.addFnAttr("target-features", own.empty() ? features.str() : (features + "," + own).str());
    }
  }
}

auto spp::codegen::EmitObjectFile(
  llvm::Module &llvm_mod, char const *path, const unsigned opt_level) -> bool {
  auto *machine = SelectedTargetMachine();
  if (machine == nullptr) { return false; }

  // The machine is built once and shared, so its level is set
  // for each emission: one process can build at several (the
  // test suite does). Left alone it is LLVM's default (O2) for
  // every mode, so dev paid for a full backend it did not want.
  machine->setOptLevel([opt_level] {
    switch (opt_level) {
      case 0: return llvm::CodeGenOptLevel::None;
      case 1: return llvm::CodeGenOptLevel::Less;
      case 2: return llvm::CodeGenOptLevel::Default;
      default: return llvm::CodeGenOptLevel::Aggressive;
    }
  }());

  // Check we can open the given path.
  auto ec = std::error_code();
  auto out = llvm::raw_fd_ostream(path, ec, llvm::sys::fs::OF_None);
  if (ec) {
    llvm::errs() << "Could not open " << path << ": " << ec.message() << "\n";
    return false;
  }

  // The legacy pass manager, because that is the only one
  // the code generator is driven through. The target's own
  // analyses go in first, as clang adds them: left to the
  // pipeline, each is built for an empty triple. No object
  // changed for it on LLVM 23, but a pass that reads them
  // would see the wrong target. LLVM 24's
  // "runCodeGenPipeline" does all of this (upstream-watch U1).
  auto codegen_pm = llvm::legacy::PassManager();
  const auto &triple = machine->getTargetTriple();
  const auto &options = machine->Options;
  codegen_pm.add(llvm::createTargetTransformInfoWrapperPass(machine->getTargetIRAnalysis()));
  codegen_pm.add(new llvm::TargetLibraryInfoWrapperPass(llvm::TargetLibraryInfoImpl(triple, options.VecLib)));
  codegen_pm.add(new llvm::RuntimeLibraryInfoWrapper(
    triple, options.ExceptionModel, options.FloatABIType, options.EABIVersion, options.MCOptions.ABIName,
    options.VecLib));
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
