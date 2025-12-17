module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_type;
import spp.codegen.llvm_ctx;
import llvm;

namespace spp::analyse::scopes {
    SPP_EXP_CLS struct TypeSymbol;
}

namespace spp::asts {
    SPP_EXP_CLS struct ClassPrototypeAst;
    SPP_EXP_CLS struct FunctionPrototypeAst;
}


namespace spp::codegen {
    SPP_EXP_FUN auto register_llvm_type_info(
        asts::ClassPrototypeAst const *cls_proto,
        LLvmCtx const *ctx)
        -> void;

    SPP_EXP_FUN auto llvm_type(
        analyse::scopes::TypeSymbol const &type_sym,
        LLvmCtx const *ctx)
        -> llvm::Type*;
}
