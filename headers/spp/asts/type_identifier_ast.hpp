#ifndef TYPE_IDENTIFIER_HPP
#define TYPE_IDENTIFIER_HPP

#include <spp/asts/type_ast.hpp>
#include <spp/asts/_fwd.hpp>


/**
 * The TypeIdentifierAst is a type expression that is represented by a single type name, and is analogous to the
 * IdentifierAst of the ExpressionAst.
 */
struct spp::asts::TypeIdentifierAst final : TypeAst {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The name for the type. This is the name of the type, such as @c Str or @code Vec[BigInt]@endcode.
     */
    icu::UnicodeString name;

    /**
     * The generic arguments for the type. This is a list of generic arguments that are used to instantiate the type.
     */
    std::unique_ptr<GenericArgumentGroupAst> generic_args;

    /**
     * Construct the TypeIdentifier with the arguments matching the members.
     * @param[in] name The name for the type.
     * @param[in] generic_args The generic arguments for the type.
     */
    explicit TypeIdentifierAst(
        decltype(name) &&name,
        decltype(generic_args) &&generic_args);

    static auto from_identifier(IdentifierAst const &identifier) -> std::unique_ptr<TypeIdentifierAst>;
};


#endif //TYPE_IDENTIFIER_HPP
