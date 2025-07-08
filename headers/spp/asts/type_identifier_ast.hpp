#ifndef TYPE_IDENTIFIER_HPP
#define TYPE_IDENTIFIER_HPP

#include <spp/asts/type_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The TypeIdentifierAst is a type expression that is represented by a single type name, and is anologus to the
 * IdentifierAst of the ExpressionAst.
 */
struct spp::asts::TypeIdentifierAst final : TypeAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The name for the type. This is the name of the type, such as @c Str or @c Vec[BigInt].
     */
    std::unique_ptr<GenericIdentifierAst> name;

    /**
     * Construct the TypeIdentifier with the arguments matching the members.
     * @param name The name for the type.
     */
    explicit TypeIdentifierAst(
        decltype(name) &&name);

    static auto from_identifier(IdentifierAst const& identifier) -> std::unique_ptr<TypeIdentifierAst>;

    static auto from_generic_identifier(GenericIdentifierAst const& identifier) -> std::unique_ptr<TypeIdentifierAst>;
};


#endif //TYPE_IDENTIFIER_HPP
