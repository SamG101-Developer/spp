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
}
