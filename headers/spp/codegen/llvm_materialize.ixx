module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_materialize;
import spp.codegen.llvm_ctx;
import llvm;

namespace spp::analyse::scopes {
  SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts::meta {
  SPP_EXP_CLS struct CompilerMetaData;
}

namespace spp::asts {
  SPP_EXP_CLS struct ExpressionAst;
  SPP_EXP_CLS struct IdentifierAst;
}

namespace spp::codegen {
  SPP_EXP_FUN auto llvm_materialize(
    asts::ExpressionAst &ast,
    analyse::scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta,
    LLvmCtx *ctx)
    -> asts::IdentifierAst*;

  /**
   * Get the address of the storage an expression names, which is what a borrow of it lowers to. A member access
   * generates the address of its field, a symbolic expression uses its symbol's allocation, and anything else (a call
   * result, a literal) is materialized into a temporary so that there is an address to hand out.
   * @param[in] ast The expression to take the address of.
   * @param[in] sm The scope manager.
   * @param[in] meta The compiler meta data.
   * @param[in] ctx The llvm context to generate into.
   * @return The pointer to the expression's storage.
   */
  SPP_EXP_FUN auto llvm_addr_of(
    asts::ExpressionAst &ast,
    analyse::scopes::ScopeManager *sm,
    asts::meta::CompilerMetaData *meta,
    LLvmCtx *ctx)
    -> llvm::Value*;
}
