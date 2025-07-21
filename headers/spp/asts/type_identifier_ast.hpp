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
     * @param[in] pos The position of the type in the source code.
     * @param[in] name The name for the type.
     * @param[in] generic_args The generic arguments for the type.
     */
    explicit TypeIdentifierAst(
        std::size_t pos,
        decltype(name) &&name,
        decltype(generic_args) &&generic_args);

    static auto from_identifier(IdentifierAst const &identifier) -> std::unique_ptr<TypeIdentifierAst>;

private:
    std::size_t m_pos;

public:
    auto ns_parts() const -> std::vector<IdentifierAst const *> override;

    auto type_parts() const -> std::vector<TypeIdentifierAst const *> override;

    auto without_generics() const -> std::unique_ptr<TypeAst> override;

    auto get_convention() const -> ConventionAst* override;

    auto substitute_generics(std::vector<GenericArgumentAst*> &&args) const -> std::unique_ptr<TypeAst> override;

    auto contains_generic(TypeAst const *generic) const -> bool override;

    auto set_generics(std::unique_ptr<GenericArgumentGroupAst> &&arg_group) -> std::unique_ptr<TypeAst> override;

    auto with_convention() const -> std::unique_ptr<TypeAst> override;
};


#endif //TYPE_IDENTIFIER_HPP
