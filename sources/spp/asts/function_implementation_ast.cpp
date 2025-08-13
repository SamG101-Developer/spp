#include <spp/asts/function_implementation_ast.hpp>
#include <spp/asts/statement_ast.hpp>
#include <spp/asts/token_ast.hpp>


auto spp::asts::FunctionImplementationAst::new_empty() -> std::unique_ptr<FunctionImplementationAst> {
    return std::make_unique<FunctionImplementationAst>();
}


auto spp::asts::FunctionImplementationAst::stage_7_analyse_semantics(
    ScopeManager *sm,
    mixins::CompilerMetaData *meta)
    -> void {
    // Analyse the semantics of the inner scope.
}
