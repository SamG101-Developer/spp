#pragma once

/**
 * Deliberately a plain header, not a module interface, because there's a TU-local entity reached.
 */

namespace spp::codegen {
  /**
   * The one target every module is built for. Hard-coded rather than read from the host, because cross-compilation
   * has to name a target either way and the answer wants to be the same in every place that asks.
   */
  constexpr auto kTargetTriple = "x86_64-pc-linux-gnu";

  /**
   * The data layout string of @c kTargetTriple : how wide each type is, what it is aligned to, and how a struct's
   * fields are packed. Every module has to carry it, because without one llvm falls back to a default layout that is
   * not the target's, and every size and offset computed from a module - a struct's field offsets, the byte count of
   * a "dereferenceable", the size an allocation asks for - is computed against whatever the module says.
   * @return The layout string, or an empty string if the target is not registered in this build of llvm.
   */
  auto HostDataLayoutString() -> char const*;

  /**
   * Emit @p llvm_module as a native object file at @p path .
   * @param[in] llvm_module The @c llvm::Module to emit, as an opaque pointer.
   * @param[in] path Where to write the object file.
   * @return @c true if the object file was written.
   */
  auto EmitObjectFile(void *llvm_module, char const *path) -> bool;

  /**
   * Drop calls to @c llvm.lifetime.* declarations that llvm no longer recognises as intrinsics.
   *
   * @n
   * Workaround, not a fix. Intrinsic names are being built with trailing garbage in this build - "llvm.lifetime.end"
   * comes out as "llvm.lifetime.endntry\0\0..." - and a name that matches no intrinsic is emitted as a call to an
   * ordinary external symbol, which nothing defines, so the link fails. A lifetime marker carries no semantics of its
   * own (it only tells the optimizer when a stack slot is dead), so removing the calls costs nothing but stack-slot
   * sharing, where leaving them costs the executable.
   *
   * @n
   * The real problem is upstream: a string handed to llvm is read back with the wrong length, which also shows up in
   * printed attribute lists and in the module's own target triple. Once that is resolved this should go.
   *
   * @param[in,out] llvm_module The @c llvm::Module to scrub, as an opaque pointer.
   * @return How many calls were dropped.
   */
  auto ScrubCorruptLifetimeIntrinsics(void *llvm_module) -> unsigned long;

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
   * Run the full optimization pipeline over a module. Separate from the coroutine lowering above, which is a
   * correctness step that has to happen either way - this one only changes how optimal the resulting codegen is.
   * @param llvm_module The @c llvm::Module to run over, as an opaque pointer (see the note above).
   */
  auto RunOptimizationPipeline(void *llvm_module) -> void;

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
