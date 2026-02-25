module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_optional_ast;
import spp.asts.function_parameter_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ExpressionAst;
    SPP_EXP_CLS struct FunctionParameterOptionalAst;
}


/**
 * The FunctionParameterOptionalAst represents an optional parameter in a function prototype. It is used to define
 * parameters that are not required, and can be omitted when calling the function.
 */
SPP_EXP_CLS struct spp::asts::FunctionParameterOptionalAst final : FunctionParameterAst {
    std::unique_ptr<ExpressionAst> default_val;

    explicit FunctionParameterOptionalAst(
        decltype(var) &&var,
        decltype(tok_colon) &&tok_colon,
        decltype(type) type,
        decltype(default_val) &&default_val);
    ~FunctionParameterOptionalAst() override;
    auto to_rust() const -> std::string override;
};
