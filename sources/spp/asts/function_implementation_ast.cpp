#include <spp/asts/function_implementation_ast.hpp>
#include <spp/asts/statement_ast.hpp>
#include <spp/asts/token_ast.hpp>


auto spp::asts::FunctionImplementationAst::new_empty() -> std::unique_ptr<FunctionImplementationAst> {
    return std::make_unique<FunctionImplementationAst>();
}


spp::asts::FunctionImplementationAst::~FunctionImplementationAst() = default;
