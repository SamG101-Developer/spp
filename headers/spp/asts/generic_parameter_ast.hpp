#ifndef GENERIC_PARAMETER_AST_HPP
#define GENERIC_PARAMETER_AST_HPP

#include <spp/asts/ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The GenericParameterAst is the base class for all generic parameters. It is inherited by the GenericParameterCompAst
 * and GenericParameterTypeAst, which represent the two types of generic parameters in the language.
 */
struct spp::asts::GenericParameterAst : virtual Ast {
    using Ast::Ast;

    /**
     * The name of the generic type parameter. This is the name that will be used to refer to the type parameter in the
     * generic type.
     */
    std::shared_ptr<TypeAst> name;

    explicit GenericParameterAst(std::shared_ptr<TypeAst> name);
};


#endif //GENERIC_PARAMETER_AST_HPP
