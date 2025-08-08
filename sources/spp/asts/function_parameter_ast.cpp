#include <spp/asts/function_parameter_ast.hpp>
#include <spp/asts/let_statement_initialized_ast.hpp>
#include <spp/asts/local_variable_ast.hpp>
#include <spp/asts/token_ast.hpp>
#include <spp/asts/type_ast.hpp>


spp::asts::FunctionParameterAst::FunctionParameterAst(
    decltype(var) &&var,
    decltype(tok_colon) &&tok_colon,
    decltype(type) &&type):
    var(std::move(var)),
    tok_colon(std::move(tok_colon)),
    type(std::move(type)) {
}


spp::asts::FunctionParameterAst::~FunctionParameterAst() = default;
