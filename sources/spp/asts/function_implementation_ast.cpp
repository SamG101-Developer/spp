#include <spp/asts/function_implementation_ast.hpp>
#include <spp/asts/statement_ast.hpp>
#include <spp/asts/token_ast.hpp>


auto spp::asts::FunctionImplementationAst::new_empty()
    -> std::unique_ptr<FunctionImplementationAst> {
    return std::make_unique<FunctionImplementationAst>();
}


spp::asts::FunctionImplementationAst::~FunctionImplementationAst() = default;


auto spp::asts::FunctionImplementationAst::clone() const
    -> std::unique_ptr<Ast> {
    auto *c = dynamic_cast<InnerScopeAst*>(InnerScopeAst::clone().release());
    return std::make_unique<FunctionImplementationAst>(
        std::move(c->tok_l),
        std::move(c->members),
        std::move(c->tok_r));
}
