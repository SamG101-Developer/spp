module;
#include <spp/macros.hpp>

export module spp.asts.function_call_argument_group_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct FunctionCallArgumentAst;
    SPP_EXP_CLS struct FunctionCallArgumentGroupAst;
}


/**
 * The FunctionCallArgumentGroupAst represents a group of function call arguments. It is used to group multiple
 * positional or keyword arguments together in a function call.
 */
SPP_EXP_CLS struct spp::asts::FunctionCallArgumentGroupAst final : Ast {
    std::vector<std::unique_ptr<FunctionCallArgumentAst>> args;

    explicit FunctionCallArgumentGroupAst(
        decltype(args) &&args);
    ~FunctionCallArgumentGroupAst() override;
    auto to_rust() const -> std::string override;
};
