#include <spp/asts/function_parameter_ast.hpp>


spp::asts::FunctionParameterAst::FunctionParameterAst(
        decltype(var) &&var,
    decltype(tok_colon) &&tok_colon,
    decltype(type) &&type):
    Ast(pos),
    var(std::move(var)),
    tok_colon(std::move(tok_colon)),
    type(std::move(type)) {
}
