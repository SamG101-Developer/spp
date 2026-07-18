export module spp.analyse.utils.builtins;
import spp.analyse.utils.cmp_utils;
import spp.utils.functions;
import spp.utils.types;
import std;
import ankerl;

namespace spp::analyse::utils::builtins {
    struct LoweredFuncImpl {
        // Unique<spp::utils::functions::CallableBase> llvm_fn;
        Unique<cmp_utils::CmpFn> cmp_fn;
    };

    auto MakeBuiltinFuncMap() -> ankerl::unordered_dense::map<Str, LoweredFuncImpl>;

    export const auto kBuiltinFuncs = MakeBuiltinFuncMap();
}
