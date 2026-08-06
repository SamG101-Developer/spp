#pragma once

/**
 * Deliberately a plain header, not a module interface, because there's a TU-local entity reached.
 */

namespace spp::codegen {
  /**
   * Lower the coroutine intrinsics in a module into real state machines, and move generator frames into their callers.
   * Not an optimization step: without it the "llvm.coro.*" intrinsics survive into the emitted module, and every
   * coroutine traps on entry for want of a frame.
   * @param llvm_module The @c llvm::Module to run over, as an opaque pointer (see the note above).
   */
  auto RunCoroLoweringPipeline(void *llvm_module) -> void;
}
