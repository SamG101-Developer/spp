module;
#include <spp/macros.hpp>

export module spp.codegen.llvm_type_registration;
import spp.codegen.llvm_ctx;

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
        LLvmCtx *ctx)
        -> void;
}
