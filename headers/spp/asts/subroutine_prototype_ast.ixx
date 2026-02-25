module;
#include <spp/macros.hpp>

export module spp.asts.subroutine_prototype_ast;
import spp.asts.function_prototype_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct SubroutinePrototypeAst;
}


SPP_EXP_CLS struct spp::asts::SubroutinePrototypeAst final : FunctionPrototypeAst {
    using FunctionPrototypeAst::FunctionPrototypeAst;
    ~SubroutinePrototypeAst() override;
    auto to_rust() const -> std::string override;
};
