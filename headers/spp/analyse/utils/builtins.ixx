export module spp.analyse.utils.builtins;
import spp.utils.functions;
import spp.analyse.utils.cmp_utils;
import std;


namespace spp::analyse::utils::builtins {
    struct LoweredFuncImpl {
        // std::unique_ptr<spp::utils::functions::CallableBase> llvm_fn;
        std::unique_ptr<cmp_utils::CmpFn> cmp_fn;
    };

    auto make_builtin_functions_map() -> std::unordered_map<std::string, LoweredFuncImpl>;

    export const auto BUILTIN_FUNCS = make_builtin_functions_map();
}
