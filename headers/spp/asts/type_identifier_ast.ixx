module;
#include <spp/macros.hpp>

export module spp.asts.type_identifier_ast;
import spp.asts.type_ast;
import genex;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}


/**
 * The TypeIdentifierAst is a type expression that is represented by a single type name, and is analogous to the
 * IdentifierAst of the ExpressionAst.
 */
SPP_EXP_CLS struct spp::asts::TypeIdentifierAst final : TypeAst {
    /**
     * The name for the type. This is the name of the type, such as @c Str or @code Vec[BigInt]@endcode.
     */
    std::string name;

    /**
     * The generic arguments for the type. This is a list of generic arguments that are used to instantiate the type.
     */
    std::unique_ptr<GenericArgumentGroupAst> generic_arg_group;

    /**
     * Construct the TypeIdentifier with the arguments matching the members.
     * @param[in] pos The position of the type in the source code.
     * @param[in] name The name for the type.
     * @param[in] generic_arg_group The generic arguments for the type.
     */
    explicit TypeIdentifierAst(
        std::size_t pos,
        decltype(name) &&name,
        decltype(generic_arg_group) generic_arg_group);

    ~TypeIdentifierAst() override;

protected:
    auto equals(ExpressionAst const &other) const -> std::strong_ordering override;

    auto equals_type_identifier(TypeIdentifierAst const &other) const -> std::strong_ordering override;

public:
    SPP_AST_KEY_FUNCTIONS;

    static auto from_identifier(IdentifierAst const &identifier) -> std::shared_ptr<TypeIdentifierAst>;

    static auto from_string(std::string const &identifier) -> std::shared_ptr<TypeIdentifierAst>;

    SPP_ATTR_ALWAYS_INLINE auto operator<=>(const TypeIdentifierAst &that) const -> std::strong_ordering {
        return equals(that);
    }

    SPP_ATTR_ALWAYS_INLINE auto operator==(const TypeIdentifierAst &that) const -> bool {
        return equals(that) == std::strong_ordering::equal;
    }

private:
    std::size_t m_pos;

    bool m_is_never_type;

public:
    auto iterator() const -> genex::generator<std::shared_ptr<const TypeIdentifierAst>> override;

    auto is_never_type() const -> bool override;

    auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> override;

    auto ns_parts() -> std::vector<std::shared_ptr<IdentifierAst>> override;

    auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> override;

    auto type_parts() -> std::vector<std::shared_ptr<TypeIdentifierAst>> override;

    auto without_convention() const -> std::shared_ptr<const TypeAst> override;

    auto get_convention() const -> ConventionAst* override;

    auto with_convention(std::unique_ptr<ConventionAst> &&conv) const -> std::shared_ptr<TypeAst> override;

    auto without_generics() const -> std::shared_ptr<TypeAst> override;

    auto substitute_generics(std::vector<GenericArgumentAst*> const &args) const -> std::shared_ptr<TypeAst> override;

    auto contains_generic(GenericParameterAst const &generic) const -> bool override;

    auto with_generics(std::unique_ptr<GenericArgumentGroupAst> &&arg_group) const -> std::shared_ptr<TypeAst> override;

    auto stage_4_qualify_types(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto stage_7_analyse_semantics(ScopeManager *sm, CompilerMetaData *meta) -> void override;

    auto infer_type(ScopeManager *sm, CompilerMetaData *meta) -> std::shared_ptr<TypeAst> override;
};
