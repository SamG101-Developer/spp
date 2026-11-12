module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_jump_tables;
import llvm;
import std;

namespace spp::codegen {
  /// Build every switch in a "llvm::Module" so it cannot jump through an entry
  /// past the end of a jump table, even speculatively, on every target. This is
  /// handrolled because LLVM's default jump protection breaks lots of cross
  /// compile targets, and caused lots of issues.
  SPP_EXP_FUN auto ApplyHardenedJumpTables(llvm::Module &llvm_mod) -> void;
}
