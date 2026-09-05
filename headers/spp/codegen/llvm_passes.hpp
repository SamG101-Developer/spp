#pragma once

/**
 * Deliberately a plain header, not a module interface, because there's a TU-local entity reached.
 */

namespace spp::codegen {
  /**
   * Choose the triple every module is built for. Must be called before any code generation: the target machine, the
   * triple and the data layout are each resolved once, on first use, and a selection made after that is ignored
   * rather than honoured - which would leave modules describing one target and the object describing another.
   *
   * @n
   * The triple is normalised, so a short name ("riscv64", "aarch64") is accepted on the same terms as a full triple.
   * A target whose backend this llvm was not built with is rejected here, with the list of the ones it does have,
   * rather than at the point an object cannot be emitted for it.
   *
   * @param[in] triple The target triple, or null/empty for the host.
   * @return @c true if the target was registered and selected.
   */
  auto SelectTarget(char const *triple) -> bool;

  /**
   * Whether the selected target is the machine this compiler is running on. Only a host build can be linked and run:
   * the linker driver invoked below is the host's, and the ffi runtimes a project ships are host objects.
   */
  auto TargetIsHost() -> bool;

  /**
   * The selected target as a directory name, which is what names its folder in the "out" tree. Validated as a single
   * path component by @c SelectTarget , and deliberately not read back off llvm - see the note on the definition.
   * @return The target's folder name; never empty, because an unselected target is the host's own triple.
   */
  auto TargetFolderName() -> char const*;

  /**
   * Stamp @p llvm_module with the selected target's triple and data layout.
   * @param[in,out] llvm_module The @c llvm::Module to stamp, as an opaque pointer.
   */
  auto ApplyTargetToModule(void *llvm_module) -> void;

  /**
   * Give every function in @p llvm_module a stack protector.
   * @param[in,out] llvm_module The @c llvm::Module to stamp, as an opaque pointer.
   * @return How many functions were given one.
   */
  auto ApplyStackProtector(void *llvm_module) -> unsigned long;

  /**
   * Emit @p llvm_module as a native object file at @p path .
   * @param[in] llvm_module The @c llvm::Module to emit, as an opaque pointer.
   * @param[in] path Where to write the object file.
   * @return @c true if the object file was written.
   */
  auto EmitObjectFile(void *llvm_module, char const *path) -> bool;

  /**
   * Check that llvm can still spell an overloaded intrinsic's name, and stop if it cannot.
   *
   * @n
   * The names arrive from @c Intrinsic::getName as a @c std::string, and there is a libstdc++ incompatibility that
   * makes that come back padded with uninitialised bytes (see @c libstdcxx_string_compat.cpp ). While it bites, no
   * overloaded intrinsic can be named correctly and every module holding one is rejected - by the verifier, whose
   * complaint is about the name and says nothing about why the name is wrong. This asks for one name and compares
   * it, so the failure is reported where the cause is rather than several layers downstream.
   *
   * @n
   * Runs its check once per process, so it costs nothing to call wherever it is convenient. The diagnosis is printed
   * in any build; the assertion that stops on it is a debug one.
   */
  auto AssertIntrinsicNamingIsSound() -> void;

  /**
   * Add a C @c main to @p llvm_module that calls @p spp_main_name , so the program has an entry point of the shape a
   * linker and a libc start-up expect. The s++ entry point cannot be it directly: its name is mangled, and it takes
   * its arguments as an s++ @c Vec[Str] rather than as @c (argc, argv) .
   *
   * @param[in,out] llvm_module The combined @c llvm::Module, as an opaque pointer.
   * @param[in] spp_main_name The linkage name of the s++ entry point.
   * @return @c true if the entry point was added.
   */
  auto EmitCEntryPoint(void *llvm_module, char const *spp_main_name) -> bool;

  /**
   * Lower the coroutine intrinsics in a module into real state machines, and move generator frames into their callers.
   * Not an optimization step: without it the "llvm.coro.*" intrinsics survive into the emitted module, and every
   * coroutine traps on entry for want of a frame.
   * @param llvm_module The @c llvm::Module to run over, as an opaque pointer (see the note above).
   */
  auto RunCoroLoweringPipeline(void *llvm_module) -> void;

  /**
   * Run the optimization pipeline over a module. Separate from the coroutine lowering above, which is a correctness
   * step that has to happen either way - this one only changes how optimal the resulting codegen is.
   * @param llvm_module The @c llvm::Module to run over, as an opaque pointer (see the note above).
   * @param opt_level 0 for none, through to 3 for full. Level 0 still runs a pipeline, because "no optimization" is
   * itself a pipeline in llvm ("buildO0DefaultPipeline"), and the per-module builder rejects being asked for O0.
   */
  auto RunOptimizationPipeline(void *llvm_module, unsigned opt_level) -> void;

  /**
   * Copy @p src_module into @p dest_module, so that the optimizer can see across what were separate modules. Every
   * s++ source file is its own llvm module, which leaves a call into another file as a bare declaration - nothing to
   * inline, however small the callee is. Linking the modules together is what turns those into real definitions, and
   * is the whole of what "full lto" is: one module, then the ordinary pipeline over it.
   *
   * @n
   * The source is copied rather than moved, because linking consumes what it is given and the per-module ir is still
   * written out afterwards for reading.
   *
   * @param[in,out] dest_module The combined @c llvm::Module being built up, as an opaque pointer.
   * @param[in] src_module The @c llvm::Module to copy in, as an opaque pointer. Must share the destination's context.
   * @return @c true if the link succeeded; @c false leaves the destination in an unspecified state.
   */
  auto LinkIntoLtoModule(void *dest_module, void *src_module) -> bool;

  /**
   * Give every definition in the combined module internal linkage, except the ones named in @p preserved_names, then
   * drop whatever nothing reaches. Everything is emitted with external linkage, because until the modules are
   * combined any of them may be the one calling into another; once they are combined that is no longer true, and the
   * optimizer is otherwise obliged to keep - and to pessimize around - every function on the chance that something
   * outside the program calls it.
   *
   * @n
   * Declarations are left alone: an @c \@ffi function has no body here, and its linkage is what lets the real one be
   * found at link time.
   *
   * @param[in,out] llvm_module The combined @c llvm::Module, as an opaque pointer.
   * @param[in] preserved_names The names to keep externally visible - the program's entry point, and nothing else if
   * there is no other way into it.
   * @param[in] preserved_count How many names @p preserved_names holds.
   */
  auto RunInternalizePass(
    void *llvm_module, char const *const *preserved_names, unsigned long preserved_count) -> void;
}
