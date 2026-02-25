module;
#include <spp/macros.hpp>

export module spp.asts.function_parameter_ast;
import spp.asts.ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct FunctionParameterAst;
    SPP_EXP_CLS struct LocalVariableAst;
    SPP_EXP_CLS struct TokenAst;
    SPP_EXP_CLS struct TypeAst;
}


/**
 * The FunctionParameterAst provides a common base to all parameter types in a function prototype. It is inherited by
 * the required, optional, variadic and self parameters, and provides the common functionality for all of them.
 */
SPP_EXP_CLS struct spp::asts::FunctionParameterAst : Ast {
    std::unique_ptr<LocalVariableAst> var;
    std::unique_ptr<TokenAst> tok_colon;
    std::unique_ptr<TypeAst> type;

    explicit FunctionParameterAst(
        decltype(var) &&var,
        decltype(tok_colon) &&tok_colon,
        decltype(type) &&type);
    ~FunctionParameterAst() override;
};
