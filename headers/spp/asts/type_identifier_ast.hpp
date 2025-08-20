#ifndef TYPE_IDENTIFIER_HPP
#define TYPE_IDENTIFIER_HPP

#include <spp/asts/type_ast.hpp>
#include <spp/asts/_fwd.hpp>

#include <genex/generator.hpp>


/**
 * The TypeIdentifierAst is a type expression that is represented by a single type name, and is analogous to the
 * IdentifierAst of the ExpressionAst.
 */
struct spp::asts::TypeIdentifierAst final : TypeAst, std::enable_shared_from_this<TypeIdentifierAst> {
    SPP_AST_KEY_FUNCTIONS;

    /**
     * The name for the type. This is the name of the type, such as @c Str or @code Vec[BigInt]@endcode.
     */
    std::string name;

    /**
     * The generic arguments for the type. This is a list of generic arguments that are used to instantiate the type.
     */
    std::shared_ptr<GenericArgumentGroupAst> generic_arg_group;

    /**
     * Construct the TypeIdentifier with the arguments matching the members.
     * @param[in] pos The position of the type in the source code.
     * @param[in] name The name for the type.
     * @param[in] generic_args The generic arguments for the type.
     */
    explicit TypeIdentifierAst(
        std::size_t pos,
        decltype(name) &&name,
        decltype(generic_arg_group) &&generic_args);

    static auto from_identifier(IdentifierAst const &identifier) -> std::unique_ptr<TypeIdentifierAst>;

private:
    std::size_t m_pos;

    bool m_is_never_type;

public:
    auto operator==(TypeIdentifierAst const &other) const -> bool;

    auto iterator() const -> genex::generator<std::shared_ptr<TypeIdentifierAst>> override;

    auto is_never_type() const -> bool override;

    auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> override;

    auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> override;

    auto without_convention() const -> std::shared_ptr<const TypeAst> override;

    auto get_convention() const -> ConventionAst* override;

    auto with_convention(std::unique_ptr<ConventionAst> &&conv) const -> std::unique_ptr<TypeAst> override;

    auto without_generics() const -> std::unique_ptr<TypeAst> override;

    auto substitute_generics(std::vector<GenericArgumentAst*> args) const -> std::unique_ptr<TypeAst> override;

    auto contains_generic(GenericParameterAst const &generic) const -> bool override;

    auto match_generic(TypeAst const &other, TypeIdentifierAst const &generic_name) const -> const ExpressionAst* override;

    auto with_generics(std::shared_ptr<GenericArgumentGroupAst> &&arg_group) const -> std::unique_ptr<TypeAst> override;

    auto stage_4_qualify_types(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, mixins::CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, mixins::CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};


#endif //TYPE_IDENTIFIER_HPP
