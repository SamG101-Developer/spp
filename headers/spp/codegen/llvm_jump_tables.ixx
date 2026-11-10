module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_jump_tables;
import std;

namespace spp::codegen {
  /// Build every switch in an "llvm::Module" (passed as an opaque
  /// pointer) so it cannot jump through an entry past the end of a
  /// jump table, even speculatively, on every target.
  ///
  /// LLVM never builds a table itself ("no-jump-tables" on every
  /// function). A dense switch (LLVM's own thresholds: at least 4
  /// cases, at least 10% of its range) becomes our own table
  /// instead: the index is masked into range without a branch, so
  /// a mispredicted bounds check still reads an entry of the table,
  /// out-of-range values reach the default through the table's last
  /// entry, and the one branch left is an "indirectbr" through it.
  /// Every other switch stays a chain of compares. The mask goes
  /// through an empty inline-asm barrier, so the backend cannot
  /// turn it back into a select it might branch on.
  ///
  /// WebAssembly has no "indirectbr": its switches stay compares.
  /// Run after the optimiser, as nothing may fold the table away.
  SPP_EXP_FUN auto ApplyHardenedJumpTables(void *llvm_module) -> void;
}
