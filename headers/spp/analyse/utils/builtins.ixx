module;
#include <spp/macros.hpp>

export module spp.analyse.utils.builtins;
import spp.analyse.utils.cmp_utils;
import spp.codegen.llvm_ctx;
import spp.utils.functions;
import spp.utils.types;
import ankerl;
import llvm;
import std;

namespace spp::analyse::scopes {
  SPP_EXP_CLS class ScopeManager;
}

namespace spp::asts {
  SPP_EXP_CLS struct FunctionPrototypeAst;
}

namespace spp::analyse::utils::builtins {
  SPP_EXP_CLS struct LoweredFuncImpl {
    Function<void(
      scopes::ScopeManager const *,
      asts::FunctionPrototypeAst const *,
      codegen::LLvmCtx *,
      llvm::Type *)> llvm_fn;
    Unique<cmp_utils::CmpFn> cmp_fn;
  };

  auto MakeBuiltinFuncMap() -> ankerl::unordered_dense::map<Str, LoweredFuncImpl>;

  export const auto kBuiltinFuncs = MakeBuiltinFuncMap();
}
