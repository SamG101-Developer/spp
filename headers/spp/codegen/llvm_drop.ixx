module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_drop;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

use(spp::analyse::scopes, class Scope);
use(spp::analyse::scopes, class ScopeManager);
use(spp::analyse::scopes, struct TypeSymbol);
use(spp::analyse::scopes, struct VariableSymbol);
use(spp::asts::meta, struct CompilerMetaData);

namespace spp::codegen {
  /// Emit the destruction of the value at a "ptr", which is
  /// the pointer to storage holding a live value of the type
  /// that the "type_sym" describes. A type superimposing Drop
  /// has its own destructor to run first, and then every field
  /// is dropped in reverse order, recursively, running their
  /// drop functions or fields recursively, etc.
  SPP_EXP_FUN auto EmitDrop(
    TypeSymbol const &type_sym,
    llvm::Value *ptr,
    ScopeManager *sm,
    CompilerMetaData *meta,
    LlvmCtx *ctx)
    -> void;
}
