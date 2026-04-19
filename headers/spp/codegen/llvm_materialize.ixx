module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_materialize;
import spp.asts;
import spp.analyse.scopes;
import spp.codegen.llvm_ctx;
import llvm;

namespace spp::codegen {
    SPP_EXP_FUN auto llvm_materialize(
        asts::ExpressionAst &ast,
        analyse::scopes::ScopeManager *sm,
        asts::meta::CompilerMetaData *meta,
        LlvmCtx *ctx)
        -> asts::IdentifierAst*;
}
