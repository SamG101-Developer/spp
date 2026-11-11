module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_alloca;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;

/// All variable stack allocation logic that is used by multiple places in the
/// ast analysis. Currently limited to the alloca instruction generation at the
/// correct position.
namespace spp::codegen {
  /// Create a stack slot in the entry block of the function currently being
  /// generated, rather than at the builder's current insert block. Every
  /// fixed-size allocation must go through this function for 2 reasons:
  /// - LLVM's mem2reg/SROA passes only promote entry block allocas into
  ///   registers.
  /// - Allocas emitted inside a loop body execute once per iteration, growing
  ///   the stack until the function returns.
  SPP_EXP_FUN auto LlvmEntryAlloca(
    llvm::Type *type, Str const &name, LlvmCtx const *ctx) -> llvm::AllocaInst*;
}
