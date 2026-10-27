module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_harden_memory;

namespace spp::codegen {
  /// Marks a function that cannot have split stacks: one that
  /// runs before the unsafe stack is mapped ("EmitCEntryPoint"),
  /// which "ApplySafeStack" leaves whole.
  SPP_EXP_FUN inline constexpr char const kNoSplitStack[] = "spp-no-split-stack";

  /// Give every function in an "llvm::Module" (passed as an
  /// opaque pointer) a stack protector. Answers how many
  /// functions were given one.
  SPP_EXP_FUN auto ApplyStackProtector(void *llvm_module) -> unsigned long;

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
  SPP_EXP_FUN auto ApplyStackClashProtection(void *llvm_module) -> unsigned long;

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
  SPP_EXP_FUN auto ApplySafeStack(void *llvm_module) -> unsigned long;
}
