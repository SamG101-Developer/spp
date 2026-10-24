#pragma once

/// Deliberately a plain header, not a module interface: the new
/// pass manager's headers expose a TU-local entity ("HasRunOnLoopT",
/// in an anonymous namespace in "LoopPassManager.h", reached through
/// "PassBuilder.h"; llvm/llvm-project#233326), so nothing that builds
/// a pipeline can sit behind a module interface. Everything else in
/// code generation is a module.

namespace spp::codegen {
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
