module;
#include <spp/macros.hpp>
#include <spp/version.hpp>
#include <spp/codegen/llvm_passes.hpp>
#include <spp/compiler/macros.hpp>

module spp.compiler.compiler_link;
import spp.codegen.llvm_artefact;
import spp.codegen.llvm_cfi;
import spp.codegen.llvm_ctx;
import spp.codegen.llvm_entry;
import spp.codegen.llvm_harden_codegen;
import spp.codegen.llvm_harden_memory;
import spp.codegen.llvm_jump_tables;
import spp.codegen.llvm_support;
import spp.codegen.llvm_target;
import spp.compiler.ffi;
import spp.compiler.out_layout;
import spp.utils.features;
import spp.utils.files;
import spp.utils.ice;
import spp.utils.shell;
import spp.utils.stats;
import spp.utils.types;
import genex;
import llvm;
import std;

namespace spp::compiler {
  namespace {
    /// Record the size of "module" for the stats report, under "when": its
    /// functions with bodies, their basic blocks, and their instructions.
    auto RecordIrSize(llvm::Module const &module, const StrView when) -> void {
      if (not utils::stats::Enabled()) { return; }
      auto functions = 0ull;
      auto blocks = 0ull;
      auto instructions = 0ull;
      for (auto const &fn : module) {
        if (fn.isDeclaration()) { continue; }
        ++functions;
        blocks += fn.size();
        instructions += fn.getInstructionCount();
      }
      const auto prefix = Str("llvm.") + Str(when) + ".";
      utils::stats::Record(prefix + "functions", functions);
      utils::stats::Record(prefix + "blocks", blocks);
      utils::stats::Record(prefix + "instructions", instructions);
    }

    /// Record the size of the file at "path" for the stats report, under "key".
    auto RecordFileSize(Path const &path, Str const &key) -> void {
      if (not utils::stats::Enabled()) { return; }
      auto ec = std::error_code();
      const auto size = std::filesystem::file_size(path, ec);
      if (not ec) { utils::stats::Record(key, size); }
    }

    /// The command the link runs, followed by the arguments after it. Either
    /// the one "[target.linkers]" names for the target, otherwise "lld-link"
    /// for an MSVC target, "cc" for the host, and for another target its GNU
    /// cross compiler, or the newest versioned one. Nothing, reported, when
    /// none is on PATH.
    auto LinkerCommand() -> Opt<Str> {
      // Get the linker for the target, either specified in the toml file, or
      // "cc" for the host target.
      const auto folder = Str(codegen::TargetFolderName());
      auto named = utils::features::Linker(folder);
      if (named.has_value()) { return named; }

      // The object is built against the MSVC runtime ("__security_cookie",
      // "__C_specific_handler"); "cc" on Windows is whatever GNU toolchain is
      // on PATH, whose runtime has neither. lld-link finds the MSVC and Windows
      // SDK libraries itself.
      if (codegen::TargetTriple().isWindowsMSVCEnvironment()) {
        if (utils::files::FindProgram("lld-link").has_value()) { return Str("lld-link"); }
        std::cerr
          << "Error: no linker for " << folder << ": 'lld-link' is not on PATH.\n"
          << "  Install LLVM (lld-link ships with it), or name one under [target.linkers] in spp.toml, as "
          << folder << " = \"link\" from a Visual Studio developer prompt.\n";
        return Nullopt;
      }
      if (codegen::TargetIsHost()) { return Str("cc"); }

      // Create a target specific "gcc" program name, and if it is found on the
      // system, return it.
      auto gcc = Str(codegen::TargetGnuTriple()) + "-gcc";
      if (utils::files::FindProgram(gcc).has_value()) { return gcc; }

      // Otherwise, find the newest version of the gcc on the system (like
      // "xyz-gcc-14"), and use that.
      if (const auto versioned = utils::files::FindNewestVersion(gcc); versioned.has_value()) {
        return utils::files::NativeString(versioned->filename());
      }

      // If nothing was found, then output an error, and return Nullopt.
      std::cerr
        << "Error: no linker for " << folder << ": neither '" << gcc
        << "' nor '" << gcc << "-N' is on PATH.\n"
        << "  Install the target's cross compiler (on Debian or Ubuntu, 'gcc-"
        << codegen::TargetGnuTriple()
        << "'), or name one under [target.linkers] in spp.toml, as "
        << folder << " = \"...\".\n";
      return Nullopt;
    }

    /// The first of the start files the link adds (the C entry, ".init"'s
    /// frame, gcc's constructors) that has no BTI property, by name, asked of
    /// the linker driver ("-print-file-name"): the linker keeps a program's
    /// property only if every object has it, and some toolchains build these
    /// without it (Ubuntu 24.04's glibc "crti.o", Arm's gcc "crtbeginS.o").
    /// Nothing when all have it, or the driver cannot say where one is.
    auto StartFileWithoutBti(Str const &linker, Path const &scratch) -> Opt<Str> {
      const auto pie = codegen::TargetIsPositionIndependent();
      for (const auto name : {pie ? "Scrt1.o" : "crt1.o", "crti.o", pie ? "crtbeginS.o" : "crtbegin.o",
             pie ? "crtendS.o" : "crtend.o", "crtn.o"}) {
        const auto query = linker + " -print-file-name=" + name + " > "
          + utils::shell::Quote(utils::files::NativeString(scratch));
        if (utils::shell::Run(query) != 0) { continue; }
        auto found = utils::files::ReadFile(scratch);
        while (not found.empty() and (found.back() == '\n' or found.back() == '\r')) { found.pop_back(); }
        if (found.empty() or found == name) { continue; }
        const auto gap = codegen::UnguardedBranchTargets(found.c_str());
        if (gap and gap->has_value()) { return Str(name); }
      }
      return Nullopt;
    }

    // ==== The LTO module ====

    /// One module holding every module of the compile, linked in, so the
    /// optimiser sees across them; verified before anything reads it.
    auto BuildLtoModule(Vec<Unique<codegen::LlvmCtx>> const &ctxs) -> Unique<llvm::Module> {
      auto lto_module = MakeUnique<llvm::Module>("spp.lto", *ctxs[0]->Context);
      codegen::ApplyTargetToModule(lto_module.get());

      // Link every module into the special lto "root" module. This creates one
      // meta-module containing all the definitions.
      for (auto const &ctx : ctxs | genex::views::ptr) {
        if (codegen::LinkIntoLtoModule(lto_module.get(), ctx->Module.get())) { continue; }
        Ice("could not link module " + ctx->Module->getName().str() + " into the lto module");
      }

      // Do a final pass on the lto module before internalizing. This checks for
      // errors in the combined module.
      auto lto_details = Str();
      auto lto_stream = llvm::raw_string_ostream(lto_details);
      if (llvm::verifyModule(*lto_module, &lto_stream)) { Ice("invalid lto module:\n" + lto_details); }
      return lto_module;
    }

    /// Dump the LLVM LTO IR into the LTO file for manual inspection. This is
    /// just for the "out" artefact.
    auto WriteLtoIr(llvm::Module const &lto_module, OutLayout const &out) -> void {
      auto ec = std::error_code();
      auto ir_out = llvm::raw_fd_ostream(
        utils::files::NativeString(out.LtoIrFile()), ec,
        static_cast<llvm::sys::fs::OpenFlags>(0));
      lto_module.print(ir_out, nullptr);
      ir_out.flush();
    }

    // ==== Hardening ====

    /// BTI, where the link can keep it: a start file without the property
    /// would take it off the program at the link, so it is left out where it is
    /// on by default (the record says so), and refused where the project asked
    /// for it. Then the landing pads, if it is still applied.
    auto ApplyBranchTargetsIfKept(llvm::Module &lto_module, Path const &object_file) -> bool {
      if (codegen::DoesTargetSupportBti()) {
        const auto linker = LinkerCommand();
        if (not linker) { return false; }
        if (const auto file = StartFileWithoutBti(*linker, object_file.parent_path() / "start-file.txt")) {
          if (utils::features::IsExplicit(utils::features::ConfigKey::BinaryCodegenBti)) {
            std::cerr
              << "Error: 'bti = true' in [binary.codegen] cannot be applied: the toolchain's " << *file
              << " has no BTI property, so no program linked with it keeps one.\n"
              << "  Use a toolchain built with branch protection, or set 'bti = false'.\n";
            return false;
          }
          utils::features::LeaveOut(
            utils::features::ConfigKey::BinaryCodegenBti, "the toolchain's " + *file + " has no BTI property");
        }
      }
      return not utils::features::IsApplied(utils::features::ConfigKey::BinaryCodegenBti, codegen::TargetIsLinux())
        or codegen::ApplyBranchTargets(&lto_module);
    }

    /// Every hardening measure the project has on, applied to the optimised
    /// program, one key each; false when one cannot be, having said why.
    auto ApplyHardening(llvm::Module &lto_module, Path const &object_file) -> bool {
      const auto m = &lto_module;
      FEATURE_GATE(MemoryStackProtect) { codegen::ApplyStackProtector(m); }
      FEATURE_GATE(MemoryStackProbe) { codegen::ApplyStackClashProtection(m); }
      FEATURE_GATE(MemoryStackSplit) { codegen::ApplySafeStack(m); }
      FEATURE_GATE(BinaryCodegenSls) { if (not codegen::ApplySlsHardening(m)) { return false; } }
      FEATURE_GATE(BinaryCodegenScrub) { if (not codegen::ApplyRegisterScrubbing(m)) { return false; } }
      FEATURE_GATE(BinaryCodegenCfi) { codegen::FinishCfi(m); }
      FEATURE_GATE(BinaryCodegenJumpTableBounds) { codegen::ApplyHardenedJumpTables(m); }
      FEATURE_GATE(BinaryCodegenFfiUnwindGuard) { if (not codegen::ApplyFfiUnwindGuard(m)) { return false; } }
      FEATURE_GATE(BinaryCodegenBti) { if (not ApplyBranchTargetsIfKept(lto_module, object_file)) { return false; } }
      FEATURE_GATE(BinaryCodegenPacRet) { if (not codegen::ApplyReturnSigning(m)) { return false; } }
      codegen::ApplyUnwindTables(m);
      return true;
    }

    /// What the program was built with, for "spp audit" to read back out of it:
    /// every on/off key, set or defaulted.
    auto EmitRecord(llvm::Module &lto_module, OutLayout const &out) -> void {
      const auto record = std::format(
        "spp {}\ntarget {}\nmode {}\n{}", SPP_VERSION, codegen::TargetFolderName(),
        ModeName(out.Mode),
        utils::features::EffectiveSettings(codegen::TargetIsLinux()));
      codegen::EmitHardeningRecord(&lto_module, record.c_str());
    }

    /// A cross build names its object first, as "spp build --target" has always
    /// printed it. Only a Linux target is linked: the link asks an ELF linker
    /// for ELF hardening. Whether to link.
    auto ReportCrossObject(Path const &object_file) -> bool {
      if (codegen::TargetIsHost()) { return true; }
      std::cout
        << "Built object for " << codegen::TargetFolderName() << ": "
        << utils::files::DisplayString(object_file) << '\n';
      if (codegen::TargetIsLinux()) { return true; }
      std::cout
        << "Not linking: only a Linux target is linked here, and "
        << codegen::TargetFolderName() << " is not one." << '\n';
      return false;
    }

    // ==== The link ====

    /// Run the link "command" that builds "exe_file", saying so; a failure is
    /// reported with the command.
    auto RunLink(Str const &command, Path const &exe_file) -> bool {
      std::cout << "Linking: " << exe_file << '\n';
      if (const auto status = utils::shell::Run(command); status != 0) {
        llvm::errs() << "Linking failed (" << status << "): " << command << "\n";
        return false;
      }
      return true;
    }

    /// Check the linked "exe_file" with "finding" (an "Unhardened..." reader):
    /// false, reported, when it cannot be read or the finding is something it
    /// was linked with, which "advice" follows.
    template <class F>
    auto CheckLinked(Path const &exe_file, F &&finding, StrView advice) -> bool {
      const auto found = finding(utils::files::NativeString(exe_file).c_str());
      if (not found) {
        std::cerr << "Error: cannot read the linked " << exe_file << ": " << found.error() << "\n";
        return false;
      }
      if (found->has_value()) {
        std::cerr << "Error: " << exe_file << " was linked with " << **found << ".\n" << advice;
        return false;
      }
      return true;
    }

    /// Code in its own segments, as "[binary.link] harden" asked of the linker.
    /// Checked in the output rather than asked of the linker: binutils before
    /// 2.39 has no "--error-rwx-segments", and before 2.31 ignores "-z
    /// separate-code" with a warning.
    auto SegmentsSeparated(Path const &exe_file) -> bool {
      return CheckLinked(
        exe_file, codegen::UnhardenedSegments,
        "  The linker does not separate code from data:"
        "use binutils 2.31 or newer (on RHEL 8, gcc-toolset-12), or set"
        "'harden = false' under [binary.link] in spp.toml.\n");
    }

    /// The BTI property kept, where it was applied. The linker keeps an
    /// object's property only when every object it links has it, and drops it
    /// with no warning otherwise; checked in the output, as the loader reads it
    /// from there.
    auto BranchTargetsKept(Path const &exe_file) -> bool {
      if (not codegen::DoesTargetSupportBti()
        or not utils::features::IsApplied(utils::features::ConfigKey::BinaryCodegenBti, codegen::TargetIsLinux())) {
        return true;
      }
      return CheckLinked(
        exe_file, codegen::UnguardedBranchTargets,
        "  An object or archive linked in was built without landing pads (gcc or clang:\n"
        "  -mbranch-protection=bti), or set 'bti = false' under [binary.codegen] in spp.toml.\n");
    }

    /// Link for an MSVC target, with lld-link's options (which link.exe shares).
    /// Each ffi library is linked through its import library ("sppc.lib") and
    /// its DLL copied beside the executable, where Windows looks for it first:
    /// there is no rpath. The C runtime is the static one, as the object names
    /// none itself ("__security_cookie" and "__C_specific_handler" come from
    /// it), so the executable needs no Visual C++ redistributable.
    auto LinkMsvcExecutable(
      OutLayout const &out, Str const &linker, Vec<Path> const &libraries) -> bool {
      const auto exe_file = out.ExecutablePath();
      auto command = linker
        + " /nologo /subsystem:console"
        + " /out:" + utils::shell::Quote(utils::files::NativeString(exe_file))
        + " " + utils::shell::Quote(utils::files::NativeString(out.ObjectFile()));

      for (auto const &lib : libraries) {
        const auto import = ffi::ImportLibraryOf(lib);
        SPP_ASSERT(import.has_value());
        std::filesystem::copy_file(
          lib, exe_file.parent_path() / lib.filename(), std::filesystem::copy_options::overwrite_existing);
        command += " " + utils::shell::Quote(utils::files::NativeString(*import));
      }
      command += " libcmt.lib libvcruntime.lib libucrt.lib kernel32.lib";
      return RunLink(command, exe_file);
    }

    /// The ffi libraries as a cc-like link takes them, each staged into "lib/"
    /// beside the executable rather than linked where it sits, so that what the
    /// loader needs travels with what was built and the path baked into it is a
    /// relative one. A versioned library keeps its version, the name its SONAME
    /// gives and so the one the loader is asked for.
    auto StageLibraries(OutLayout const &out, Vec<Path> const &libraries) -> Str {
      const auto lib_dir = out.LibRoot();
      std::filesystem::create_directories(lib_dir);

      // Every ffi library is a direct dependency on ELF, used or not. One may need
      // another ("libssl.so.4" needs "libcrypto.so.4"), and the executable's
      // "$ORIGIN/lib" runpath does not apply to a library's own dependencies: if
      // "--as-needed" (the default of many distributions' cc) dropped the one the
      // program never calls, the loader would look for it everywhere but beside the
      // executable, and pick up another build of it or none.
      const auto elf = codegen::TargetTriple().isOSBinFormatELF();
      auto args = Str();
      if (elf and not libraries.empty()) { args += " -Wl,--push-state,--no-as-needed"; }
      for (auto const &lib : libraries) {
        const auto staged = lib_dir / lib.filename();
        std::filesystem::copy_file(lib, staged, std::filesystem::copy_options::overwrite_existing);
        args += " " + utils::shell::Quote(utils::files::NativeString(staged));
      }
      if (elf and not libraries.empty()) { args += " -Wl,--pop-state"; }
      return args;
    }

    /// Binary hardening, done by attaching flags to the linker. ELF only: ld64
    /// already does each by default.
    auto LinkHardeningFlags() -> Str {
      if (not codegen::TargetTriple().isOSBinFormatELF()) { return Str(); }
      auto flags = Str();

      // Asked for whether or not hardening is on: a gcc configured to default to
      // PIE would otherwise link a fixed-address object as one.
      if (not codegen::TargetIsPositionIndependent()) { flags += " -no-pie"; }
      FEATURE_GATE(BinaryLinkHarden) {
        if (codegen::TargetIsPositionIndependent()) {
          flags += " -pie"; // No fixed load address to write an exploit against.
        }
        flags += " -Wl,-z,relro,-z,now"; // Read-only after startup, and nothing left to bind later.
        flags += " -Wl,-z,noexecstack"; // The stack is data; say so in the header rather than by accident.
        flags += " -Wl,-z,separate-code"; // Code in its own mapping, so no data shares a page with it.
        flags += " -Wl,-z,defs"; // A symbol nothing defines is a link error, not a run-time surprise.
        flags += " -Wl,-z,text"; // A text relocation is a link error, so code is never writable at load.
      }
      return flags;
    }

    /// The command a cc-like driver links the program with: the object, the
    /// staged ffi libraries, libm and libatomic where needed, the runpath to
    /// "lib/", and the hardening flags.
    auto CcLinkCommand(OutLayout const &out, Str const &linker, Vec<Path> const &libraries) -> Str {
      const auto &triple = codegen::TargetTriple();
      const auto obj_filepath = utils::files::NativeString(out.ObjectFile());
      auto command = linker
        + " -o " + utils::shell::Quote(utils::files::NativeString(out.ExecutablePath()))
        + " " + utils::shell::Quote(obj_filepath)
        + StageLibraries(out, libraries);

      // Add the "libm" math library flag for everything except COFF objects, which
      // carry their own math functions.
      if (not triple.isOSBinFormatCOFF()) { command += " -lm"; }

      // Add the "libatomic" library for linux when the compiled program actually
      // needs it (when LLVM cannot lower atomics to intrinsics - on some
      // cross-codegen esoteric systems).
      if (triple.isOSLinux() and codegen::ImportsAtomicLibcalls(obj_filepath.c_str())) {
        command += " -latomic";
      }

      // Tell the executable to look for the shared libraries in a lib folder next
      // to the executable.
      if (triple.isOSBinFormatELF()) { command += " -Wl,-rpath,'$ORIGIN/lib'"; }
      if (triple.isOSBinFormatMachO()) { command += " -Wl,-rpath,@loader_path/lib"; }
      return command + LinkHardeningFlags();
    }

    /// Link with a cc-like driver (gcc, clang, a cross gcc): ELF, Mach-O and
    /// MinGW. Each ffi library is staged into "lib/" beside the executable and
    /// found there through the runpath; on ELF the executable is then checked
    /// for the segment hardening and the BTI property it was linked to keep.
    auto LinkCcExecutable(
      OutLayout const &out, Str const &linker, Vec<Path> const &libraries) -> bool {
      const auto exe_file = out.ExecutablePath();
      if (not RunLink(CcLinkCommand(out, linker, libraries), exe_file)) { return false; }
      FEATURE_GATE(BinaryLinkHarden) { if (not SegmentsSeparated(exe_file)) { return false; } }
      FEATURE_GATE(BinaryCodegenBti) { if (not BranchTargetsKept(exe_file)) { return false; } }
      return true;
    }

    /// Link the emitted object against the ffi runtimes the project's packages
    /// ship, producing the executable. The object holds calls into the ffi
    /// runtime and nothing else external, so the link is the object plus
    /// whatever shared libraries the project's packages ship. True for a
    /// successful link.
    auto LinkExecutable(OutLayout const &out) -> bool {
      // Determine the linker command, and ensure that it exists (that a linker was
      // found).
      const auto linker = LinkerCommand();
      if (not linker.has_value()) { return false; }

      // Get the ffi libraries specifically for the target. The stub is always
      // present (and pre-checked), so the analysis can pass, but the execution
      // requires a target specific FFI shared library present, per FFI library.
      const auto libraries = ffi::Libraries(out.Root);
      if (not libraries.has_value()) { return false; }

      // musl has no fortified calls ("__memcpy_chk" and friends are glibc's), so a
      // library built against it has none to find. Check RELRO and segments for the
      // FFI libraries too.
      FEATURE_GATE(BinaryLinkFortifiedFfi, not codegen::TargetIsMusl()) {
        if (not ffi::CheckFortified(out.Root, *libraries)) { return false; }
      }
      FEATURE_GATE(BinaryLinkRelroFfi) { if (not ffi::CheckRelro(out.Root, *libraries)) { return false; } }
      FEATURE_GATE(BinaryLinkSegmentsFfi) { if (not ffi::CheckSegments(out.Root, *libraries)) { return false; } }

      // Link the way the target's linker takes it, then report the executable.
      const auto linked = codegen::TargetTriple().isWindowsMSVCEnvironment()
        ? LinkMsvcExecutable(out, *linker, *libraries)
        : LinkCcExecutable(out, *linker, *libraries);
      if (not linked) { return false; }
      std::cout << "Built executable: " << out.ExecutablePath() << '\n';
      return true;
    }
  }
}

auto spp::compiler::LinkTimeOptimize(
  Vec<Unique<codegen::LlvmCtx>> const &ctxs, const StrView entry,
  OutLayout const &out, const unsigned opt_level, const bool verify_only) -> bool {
  // Failsafe guard; this should never trigger, as there must always be one s++
  // module present for the compiler to build/run.
  if (ctxs.empty()) { return true; }
  utils::stats::BeginPhase("lto-link");
  const auto lto_module = BuildLtoModule(ctxs);

  // The unit tests stop here: the combined module is valid, and optimising,
  // emitting and linking it would add nothing more.
  if (verify_only) { return true; }

  // Internalize all the definitions in the lto module before optimizing. The C
  // entry point is added before internalizing, so that it is the thing being
  // kept and the S++ one it calls is free to be internalized into it along with
  // everything else.
  const auto has_entry_point = not entry.empty();
  if (has_entry_point) {
    codegen::EmitCEntryPoint(
      lto_module.get(), Str(entry).c_str(),
      utils::features::IsEnabled(utils::features::ConfigKey::MemoryStackSplit),
      utils::features::IsApplied(utils::features::ConfigKey::MemoryMapDenyWx, codegen::TargetIsLinux()),
      utils::features::IsApplied(utils::features::ConfigKey::MemoryMapSeal, codegen::TargetIsLinux()));
    constexpr auto preserved = "main";
    codegen::RunInternalizePass(lto_module.get(), &preserved, 1);
  }

  // Control-flow integrity types its functions before optimising, as llvm's
  // passes keep "kcfi_type" on a function they rebuild but not s++'s own
  // records; its calls are checked with the rest of the hardening below.
  FEATURE_GATE(BinaryCodegenCfi, has_entry_point) {
    if (not codegen::ApplyCfiTypes(lto_module.get())) { return false; }
  }

  // Every function is built for the chosen CPU ("[target.floor] cpu"),
  // named on it, as a function's own target features would be; and
  // sppc's allocation functions are named as such, so LLVM treats them
  // as "malloc" and "free".
  codegen::ApplyTargetToFunctions(lto_module.get());
  codegen::ApplyRuntimeAllocatorAttrs(lto_module.get());

  // Run the standard optimization pipeline across the LTO module, using the
  // optimization level from "rel" (O3) vs "dev" (O0).
  RecordIrSize(*lto_module, "before_opt");
  utils::stats::BeginPhase("optimise");
  const auto cfi = has_entry_point
    and utils::features::IsEnabled(utils::features::ConfigKey::BinaryCodegenCfi);
  codegen::RunOptimizationPipeline(
    lto_module.get(), codegen::SelectedTargetMachine(), opt_level,
    cfi ? &codegen::ApplyCfiCoroTypes : nullptr,
    utils::features::IsEnabled(utils::features::ConfigKey::BinaryCodegenAirIntegers));
  RecordIrSize(*lto_module, "after_opt");
  utils::stats::BeginPhase("write-ir-and-harden");
  WriteLtoIr(*lto_module, out);

  // Only a program gets built into something runnable. A library has no entry
  // point, so there is nothing for a linker to make an executable out of, and
  // the ir is the whole of what it produces.
  if (not has_entry_point) { return true; }

  // Harden the program, record what it was built with, and emit its object.
  const auto object_file = out.ObjectFile();
  if (not ApplyHardening(*lto_module, object_file)) { return false; }
  EmitRecord(*lto_module, out);
  utils::stats::BeginPhase("emit-object");
  if (not codegen::EmitObjectFile(
    lto_module.get(), utils::files::NativeString(object_file).c_str(), opt_level)) { return false; }
  RecordFileSize(object_file, "object.bytes");

  // Link the executable up, where the target is linked here at all.
  if (not ReportCrossObject(object_file)) { return true; }
  utils::stats::BeginPhase("link");
  const auto linked = LinkExecutable(out);
  if (linked) { RecordFileSize(out.ExecutablePath(), "executable.bytes"); }
  return linked;
}
