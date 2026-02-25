module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_variadic_ast;
import spp.asts.function_parameter_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct FunctionParameterVariadicAst;
}


/**
 * The FunctionParameterVariadicAst represents a variadic parameter in a function prototype. It is used to define
 * parameters that can accept an arbitrary number of arguments, such as @c *args in Python.
 */
SPP_EXP_CLS struct spp::asts::FunctionParameterVariadicAst final : FunctionParameterAst {
    explicit FunctionParameterVariadicAst(
        decltype(var) &&var,
        decltype(tok_colon) &&tok_colon,
        decltype(type) type);
    ~FunctionParameterVariadicAst() override;
    auto to_rust() const -> std::string override;
};
