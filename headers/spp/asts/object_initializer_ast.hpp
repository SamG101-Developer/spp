#ifndef OBJECT_INITIALIZER_AST_HPP
#define OBJECT_INITIALIZER_AST_HPP

#include <spp/asts/primary_expression_ast.hpp>
#include <spp/asts/_fwd.hpp>


struct spp::asts::ObjectInitializerAst final : PrimaryExpressionAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The type being initialized by the object initializer. This is the type of the object being created.
     */
    std::shared_ptr<TypeAst> type;

    /**
     * The object initializer argument group that contains the arguments for the object initializer. These arguments
     * will be passed into the attributes of the object being created.
     */
    std::unique_ptr<ObjectInitializerArgumentGroupAst> arg_group;

    /**
     * Construct the ObjectInitializerAst with the arguments matching the members.
     * @param type The type being initialized by the object initializer.
     * @param arg_group The object initializer argument group that contains the arguments for the object initializer.
     */
    ObjectInitializerAst(
        decltype(type) &&type,
        decltype(arg_group) &&arg_group);

    ~ObjectInitializerAst() override;
};


#endif //OBJECT_INITIALIZER_AST_HPP
