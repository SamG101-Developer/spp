#pragma once

/// Deliberately a plain header, not a module interface: the new
/// pass manager's headers expose a TU-local entity ("HasRunOnLoopT",
/// in an anonymous namespace in "LoopPassManager.h", reached through
/// "PassBuilder.h"; llvm/llvm-project#233326), so nothing that builds
/// a pipeline can sit behind a module interface. Everything else in
/// code generation is a module.

namespace spp::codegen {
  /// The bodiless declaration a call to "std::hints::cold_path"
  /// is lowered to. "ApplyColdPathHints" removes every call to
  /// it, so the name never reaches an object file.
  inline constexpr auto kColdPathMarker = "spp.hint.cold_path";

  /// Turn the "cold_path" markers in an "llvm::Module" (passed
  /// as an opaque pointer) into branch weights, then remove
  /// them. A block is cold if it holds a marker, or if every
  /// way out of it other than unwinding is cold; each branch
  /// with both a cold and a hot successor is weighted against
  /// the cold one, as "unlikely" would weight it, and a
  /// function whose entry block is cold is marked "cold". Runs
  /// on every module, at every optimisation level, before
  /// anything (coroutine splitting, inlining) moves code out
  /// of the block it was emitted into.
  auto ApplyColdPathHints(void *llvm_module) -> void;

  /// Run the optimisation pipeline over an "llvm::Module"
  /// (passed as an opaque pointer): the combined module, once
  /// every s++ module is linked into it. It also lowers the
  /// coroutines, at every level, which is a correctness step:
  /// without it the "llvm.coro.*" intrinsics reach the backend
  /// and every coroutine traps on entry (an Ice if any are
  /// left). Lowering here rather than per module is what makes
  /// iteration fast: a coroutine defined in another file (every
  /// std iterator) can only be inlined into its caller, and its
  /// frame put on the caller's stack, before it is split, and
  /// before linking its body is not there to inline. "opt_level"
  /// runs from 0 (none) to 3
  /// (full). Level 0 still runs a pipeline, because "no
  /// optimisation" is itself a pipeline in llvm
  /// ("buildO0DefaultPipeline"), and the per-module builder
  /// rejects being asked for O0.
  ///
  /// "target_machine" is the selected "llvm::TargetMachine" (an
  /// opaque pointer, from "SelectedTargetMachine"). Without it the
  /// middle-end costs everything with llvm's target-less model,
  /// which guesses: it vectorises where the real target has no
  /// cheap vector form, and unrolls where it should not (or not
  /// where it should).
  ///
  /// "after_coro_split" (may be null) is run over the module once
  /// every coroutine is split into its ".resume", ".destroy" and
  /// ".cleanup" functions, and before MergeFunctions: what types
  /// those for control-flow integrity ("ApplyCfiCoroTypes").
  ///
  /// "air_integers" runs the AIR passes ("llvm_air.hpp", O1 and
  /// up): overflow checks may move from their operation to before
  /// the wrong value would be observed, which lets loops vectorise.
  /// Every overflow that matters still aborts, with its check's
  /// kind. Off, every check stays at its operation.
  auto RunOptimizationPipeline(
    void *llvm_module, void *target_machine, unsigned opt_level,
    void (*after_coro_split)(void *llvm_module), bool air_integers) -> void;

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
