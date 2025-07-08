#ifndef LOCAL_VARIABLE_AST_HPP
#define LOCAL_VARIABLE_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::LocalVariableAst : Ast {
    using Ast::Ast;

protected:
    /**
     * The @c let statement that destructures are converted to, to introduce the variables created by the pattern.
     */
    std::unique_ptr<LetStatementInitializedAst> m_mapped_let;
};


#endif //LOCAL_VARIABLE_AST_HPP
