module;
#include <spp/macros.hpp>

export module spp.asts.coroutine_prototype_ast;
import spp.asts.function_prototype_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct CoroutinePrototypeAst;
}


SPP_EXP_CLS struct spp::asts::CoroutinePrototypeAst final : FunctionPrototypeAst {
    using FunctionPrototypeAst::FunctionPrototypeAst;
    ~CoroutinePrototypeAst() override;
    auto to_rust() const -> std::string override;
};
