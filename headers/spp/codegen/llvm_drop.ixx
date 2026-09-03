module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_drop;
import spp.codegen.llvm_ctx;
import spp.utils.types;
import llvm;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS class Scope;
  SPP_EXP_CLS class ScopeManager;
  SPP_EXP_CLS struct TypeSymbol;
  SPP_EXP_CLS struct VariableSymbol;
}

namespace spp::asts::meta {
  SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::codegen {
  /**
   * Emit the destruction of the value at @p ptr , which is a pointer to storage holding a live value of the type
   * @p type_sym describes. A type that superimposes @c Drop has its own @c drop run first - it may still read its
   * attributes - and then every attribute is destroyed in reverse declaration order, recursively.
   * @param[in] type_sym The symbol of the type of the value being destroyed.
   * @param[in] ptr A pointer to the value's storage.
   * @param[in] sm The scope manager, positioned anywhere the type resolves from.
   * @param[in] meta Associated metadata.
   * @param[in] ctx The LLVM context containing all codegen info.
   */
  SPP_EXP_FUN auto EmitDrop(
    analyse::scopes::TypeSymbol const &type_sym,
    llvm::Value *ptr,
    analyse::scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta,
    LlvmCtx *ctx)
    -> void;
}
