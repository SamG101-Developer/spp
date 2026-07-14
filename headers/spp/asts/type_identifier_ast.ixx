module;
#include <spp/macros.hpp>

export module spp.asts.type_identifier_ast;
import spp.asts.type_ast;
import spp.asts.mixins.compiler_stages;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}

COMMON_AST_IMPORTS

/**
 * The TypeIdentifierAst is a type expression that is represented by a single type name, and is analogous to the
 * IdentifierAst of the ExpressionAst.
 */
SPP_EXP_CLS struct spp::asts::TypeIdentifierAst final : TypeAst {
    SPP_GCC_VTABLE_FIX

    /**
     * The name for the type. This is the name of the type, such as @c Str or @code Vec[BigInt]@endcode.
     */
    Str Name;

    /**
     * The generic arguments for the type. This is a list of generic arguments that are used to instantiate the type.
     */
    Unique<GenericArgumentGroupAst> GnArgGroup;

    /**
     * Factory function to create a @c TypeIdentifierAst from an @c IdentifierAst node. This uses the internal string
     * inside of the @c IdentifierAst to construct the @C TypeIdentifierAst.
     * @param identifier The @c IdentifierAst node being transitioned.
     * @return The new @c TypeIdentifierAst.
     */
    static auto FromIdentifier(IdentifierAst const &identifier) -> Unique<TypeIdentifierAst>;

    /**
     * Factory function to create a @c TypeIdentifierAst from a raw @c Str node. Generics cannot be included in
     * the string as no parsing is done, just wrapping into the @C TypeIdentifierNode. Use the @c ParserSpp class
     * otherwise, with the @c INJECT_CODE macro, to parse generics.
     * @param identifier The raw string being transitioned.
     * @return The new @c TypeIdentifierAst.
     */
    static auto FromString(Str const &identifier) -> Unique<TypeIdentifierAst>;

    /**
     * Construct the TypeIdentifier with the arguments matching the members.
     * @param[in] pos The position of the type in the source code.
     * @param[in] name The name for the type.
     * @param[in] generic_arg_group The generic arguments for the type.
     */
    explicit TypeIdentifierAst(
        std::size_t pos,
        decltype(Name) &&name,
        decltype(GnArgGroup) generic_arg_group);

    ~TypeIdentifierAst() override;

    auto operator<=>(const TypeIdentifierAst &that) const -> Ordering;
    auto operator==(const TypeIdentifierAst &that) const -> bool;

    SPP_ATTR_NODISCARD auto EqualsTypeIdentifier(TypeIdentifierAst const &other) const -> Ordering override;

    SPP_ATTR_NODISCARD auto Equals(ExpressionAst const &other) const -> Ordering override;

    SPP_AST_KEY_FUNCTIONS;

    auto Stage4_QualifyTypes(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto Stage7_AnalyseSemantics(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> void override;

    auto InferType(analyse::scopes::ScopeManager *sm, meta::CompilerMetaData *meta) -> TypeAst* override;

    // SPP_ATTR_NODISCARD auto Iterator() const -> Vec<TypeIdentifierAst*> override;

    SPP_ATTR_NODISCARD auto IsNeverType() const noexcept -> bool override;

    SPP_ATTR_NODISCARD auto IsSelfType() const noexcept -> bool override;

    SPP_ATTR_NODISCARD auto NsParts() const -> Vec<IdentifierAst*> override;

    SPP_ATTR_NODISCARD auto TypeParts() const -> Vec<TypeIdentifierAst*> override;

    SPP_ATTR_NODISCARD auto WithoutConvention() const -> TypeAst* override;

    SPP_ATTR_NODISCARD auto GetConvention() const -> ConventionAst* override;

    SPP_ATTR_NODISCARD auto WithConvention(Unique<ConventionAst> &&conv) const -> Unique<TypeAst> override;

    SPP_ATTR_NODISCARD auto WithoutGenerics() const -> Unique<TypeAst> override;

    SPP_ATTR_NODISCARD auto SubstituteGenerics(Vec<GenericArgumentAst*> const &args) const -> Unique<TypeAst> override;

    SPP_ATTR_NODISCARD auto ContainsGenerics(GenericParameterAst const &generic) const -> bool override;

    SPP_ATTR_NODISCARD auto WithGenerics(Unique<GenericArgumentGroupAst> &&arg_group) const -> Unique<TypeAst> override;

    SPP_ATTR_NODISCARD auto IsCompilerGeneratedType() const -> bool override;

    auto ResetCache() -> void override;

    SPP_ATTR_NODISCARD auto IsTypeIdentifier() const noexcept -> bool override;

    auto AnkerlHash() const -> std::size_t override;

    SPP_ATTR_NODISCARD auto ToView() const -> StrView;

private:
    std::size_t _Pos;
    bool _IsNeverType = false;
    bool _IsSelfType = false;
    bool _HasAnalysed = false;
};

SPP_GCC_VTABLE_FIX_IMPL(spp::asts::TypeIdentifierAst)
