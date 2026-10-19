module;
#include <spp/macros.hpp>

export module spp.codegen.LlvmMaterialize;
import spp.codegen.llvm_ctx;
import llvm;

use(spp::analyse::scopes, class ScopeManager);
use(spp::asts::meta, struct CompilerMetaData);
use(spp::asts, struct ExpressionAst);
use(spp::asts, struct IdentifierAst);

namespace spp::codegen {
  SPP_EXP_FUN auto LlvmMaterialize(
    ExpressionAst &ast,
    ScopeManager *sm,
    CompilerMetaData *meta,
    LlvmCtx *ctx)
    -> IdentifierAst*;

  /// Get the address of the storage an expression names, which
  /// is what a borrow of it lowers to. A member access
  /// generates the address of its field, a symbolic expression
  /// uses its symbol's allocation, and anything else (a call
  /// result, a literal) is materialised into a temporary so
  /// there is an address to hand out.
  SPP_EXP_FUN auto LlvmAddrOf(
    ExpressionAst &ast,
    ScopeManager *sm,
    CompilerMetaData *meta,
    LlvmCtx *ctx)
    -> llvm::Value*;
}
