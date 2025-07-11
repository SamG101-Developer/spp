#ifndef FUNCTION_IMPLEMENTATION_AST_HPP
#define FUNCTION_IMPLEMENTATION_AST_HPP

#include <spp/asts/inner_scope_ast.hpp>
#include <spp/asts/_fwd.hpp>

namespace spp::parse { class ParserSpp; }


/**
 * The FunctionImplementationAst represents the implementation of a function. It is used to define the body of a
 * function and contains the statements that make up the function's implementation. Semantically equivalent to a basic
 * InnerScopeAst.
 */
struct spp::asts::FunctionImplementationAst final : InnerScopeAst<std::unique_ptr<FunctionMemberAst>> {
    friend class parse::ParserSpp;
    using InnerScopeAst::InnerScopeAst;

private:
    static auto new_empty() -> std::unique_ptr<FunctionImplementationAst>;
};


#endif //FUNCTION_IMPLEMENTATION_AST_HPP
