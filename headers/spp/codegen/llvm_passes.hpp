#pragma once

/// Deliberately a plain header, not a module interface,
/// because a TU-local entity is reached.

namespace spp::codegen {
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
  auto SelectTarget(char const *triple) -> bool;

  /// Whether the selected target is the machine this compiler
  /// is running on. Only a host build can be linked and run:
  /// the linker driver invoked below is the host's, and the
  /// ffi runtimes a project ships are host objects.
  auto TargetIsHost() -> bool;

  /// The selected target as a directory name, which names its
  /// folder in the "out" tree. Validated as a single path
  /// component by "SelectTarget", and deliberately not read
  /// back off llvm - see the note on the definition. Never
  /// empty, because an unselected target is the host's own
  /// triple.
  auto TargetFolderName() -> char const*;

  /// Stamp an "llvm::Module" (passed as an opaque pointer)
  /// with the selected target's triple and data layout.
  auto ApplyTargetToModule(void *llvm_module) -> void;

  /// Give every function in an "llvm::Module" (passed as an
  /// opaque pointer) a stack protector. Answers how many
  /// functions were given one.
  auto ApplyStackProtector(void *llvm_module) -> unsigned long;

  /// Give every function in an "llvm::Module" (passed as an
  /// opaque pointer) an inline stack probe, so a frame is
  /// claimed a page at a time rather than by one subtraction
  /// from the stack pointer. Without it a frame larger than
  /// the guard page below the stack can step clean over that
  /// page and land in whatever mapping follows, and the first
  /// write into the new frame hits memory the function was
  /// never given - the "stack clash" shape, and the one hole a
  /// canary cannot see, because nothing was overwritten on the
  /// way past. Answers how many functions were given one.
  auto ApplyStackClashProtection(void *llvm_module) -> unsigned long;

  /// Split every function's frame in two: the objects a callee
  /// could write past stay on a separate "unsafe" stack, and
  /// the frame the hardware knows about - the return address,
  /// the saved registers, the spills - keeps only what nothing
  /// can reach out of bounds. Which side an object goes on is
  /// llvm's own analysis of its uses: a local whose address
  /// escapes into a call that may capture or write it is
  /// unsafe, and one only ever read and written in bounds is
  /// not. Answers how many functions were stamped - not how
  /// many were split: a function with nothing unsafe in it
  /// keeps one frame, and llvm decides that per function when
  /// it lowers.
  auto ApplySafeStack(void *llvm_module) -> unsigned long;

  /// Give every function in an "llvm::Module" (passed as an
  /// opaque pointer) asynchronous unwind tables, so a stack can
  /// be walked from any instruction in it. Nothing here throws,
  /// so without them every function is "nounwind" with no
  /// ".eh_frame" entry, and the runtime's crash handler cannot
  /// see past the first s++ frame. The cost is binary size
  /// only. Answers how many functions were given one.
  auto ApplyUnwindTables(void *llvm_module) -> unsigned long;

  /// Emit an "llvm::Module" (passed as an opaque pointer) as a
  /// native object file at "path". Answers whether the object
  /// file was written.
  auto EmitObjectFile(void *llvm_module, char const *path) -> bool;

  /// Check that llvm can still spell an overloaded intrinsic's
  /// name, and stop if it cannot.
  ///
  /// The names come from "Intrinsic::getName" as a
  /// "std::string", and a libstdc++ incompatibility makes that
  /// come back padded with uninitialised bytes (see
  /// "libstdcxx_string_compat.cpp"). While it bites, no
  /// overloaded intrinsic can be named correctly and every
  /// module holding one is rejected - by the verifier, whose
  /// complaint is about the name and says nothing about why it
  /// is wrong. This asks for one name and compares it, so the
  /// failure is reported where the cause is rather than
  /// several layers downstream.
  ///
  /// Runs its check once per process, so it costs nothing to
  /// call wherever it is convenient. The diagnosis is printed
  /// in any build; the assertion that stops on it is a debug
  /// one.
  auto AssertIntrinsicNamingIsSound() -> void;

  /// Add a C "main" to the combined "llvm::Module" (passed as
  /// an opaque pointer) that calls the s++ entry point named
  /// "spp_main_name", so the program has an entry point of the
  /// shape a linker and a libc start-up expect. The s++ entry
  /// point cannot be it directly: its name is mangled, and it
  /// takes its arguments as an s++ "Vec[Str]" rather than as
  /// "(argc, argv)".
  ///
  /// "split_stacks" brings the main thread's unsafe stack up as
  /// part of the runtime start-up. It must match what
  /// "ApplySafeStack" is asked to do later: a program whose
  /// functions read an unsafe stack pointer that was never set
  /// up writes its first split frame through a null one.
  /// Answers whether the entry point was added.
  auto EmitCEntryPoint(void *llvm_module, char const *spp_main_name, bool split_stacks) -> bool;

  /// Lower the coroutine intrinsics in an "llvm::Module"
  /// (passed as an opaque pointer) into real state machines,
  /// and move generator frames into their callers. Not an
  /// optimisation step: without it the "llvm.coro.*"
  /// intrinsics survive into the emitted module, and every
  /// coroutine traps on entry for want of a frame.
  auto RunCoroLoweringPipeline(void *llvm_module) -> void;

  /// Run the optimisation pipeline over an "llvm::Module"
  /// (passed as an opaque pointer). Separate from the coroutine
  /// lowering above, which is a correctness step that has to
  /// happen either way - this one only changes how good the
  /// generated code is. "opt_level" runs from 0 (none) to 3
  /// (full). Level 0 still runs a pipeline, because "no
  /// optimisation" is itself a pipeline in llvm
  /// ("buildO0DefaultPipeline"), and the per-module builder
  /// rejects being asked for O0.
  auto RunOptimizationPipeline(void *llvm_module, unsigned opt_level) -> void;

  /// Copy "src_module" into "dest_module" (both opaque
  /// "llvm::Module" pointers, sharing one context), so the
  /// optimiser can see across what were separate modules.
  /// Every s++ source file is its own llvm module, which leaves
  /// a call into another file as a bare declaration - nothing
  /// to inline, however small the callee. Linking the modules
  /// together turns those into real definitions, and is the
  /// whole of what "full lto" is: one module, then the ordinary
  /// pipeline over it.
  ///
  /// The source is copied rather than moved, because linking
  /// consumes what it is given and the per-module ir is still
  /// written out afterwards for reading. Answers whether the
  /// link succeeded; on failure the destination is left in an
  /// unspecified state.
  auto LinkIntoLtoModule(void *dest_module, void *src_module) -> bool;

  /// Give every definition in the combined "llvm::Module"
  /// (passed as an opaque pointer) internal linkage, except the
  /// "preserved_count" names in "preserved_names", then drop
  /// whatever nothing reaches. The preserved names are the
  /// program's entry point, and nothing else if there is no
  /// other way into it. Everything is emitted with external
  /// linkage, because until the modules are combined any of
  /// them may be the one calling into another; once combined
  /// that is no longer true, and the optimiser is otherwise
  /// obliged to keep - and to pessimise around - every function
  /// on the chance that something outside the program calls
  /// it.
  ///
  /// Declarations are left alone: an "!ffi" function has no
  /// body here, and its linkage is what lets the real one be
  /// found at link time.
  auto RunInternalizePass(
    void *llvm_module, char const *const *preserved_names, unsigned long preserved_count) -> void;
}
