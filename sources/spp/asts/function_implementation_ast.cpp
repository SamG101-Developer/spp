module spp.asts.function_implementation_ast;
import spp.asts.ast;
import spp.asts.utils.ast_utils;


spp::asts::FunctionImplementationAst::~FunctionImplementationAst() = default;


auto spp::asts::FunctionImplementationAst::new_empty()
    -> std::unique_ptr<FunctionImplementationAst> {
    return std::make_unique<FunctionImplementationAst>();
}


auto spp::asts::FunctionImplementationAst::clone() const
    -> std::unique_ptr<Ast> {
    auto *c = InnerScopeAst::clone().release()->to<InnerScopeAst>();
    return std::make_unique<FunctionImplementationAst>(
        std::move(c->tok_l),
        std::move(c->members),
        std::move(c->tok_r));
}
