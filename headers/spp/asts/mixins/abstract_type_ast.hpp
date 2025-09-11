#pragma once
#include <spp/asts/_fwd.hpp>

#include <genex/generator.hpp>


namespace spp::asts {
    struct IdentifierAst;
}

namespace spp::asts::mixins {
    struct AbstractTypeAst;
}


struct spp::asts::mixins::AbstractTypeAst {
public:
    virtual ~AbstractTypeAst() = default;

public:
    virtual auto iterator() const -> genex::generator<std::shared_ptr<const TypeIdentifierAst>> = 0;

    virtual auto is_never_type() const -> bool = 0;

    virtual auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> = 0;

    virtual auto ns_parts() -> std::vector<std::shared_ptr<IdentifierAst>> = 0;

    virtual auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> = 0;

    virtual auto type_parts() -> std::vector<std::shared_ptr<TypeIdentifierAst>> = 0;

    virtual auto without_convention() const -> std::shared_ptr<const TypeAst> = 0;

    virtual auto get_convention() const -> ConventionAst* = 0;

    virtual auto with_convention(std::unique_ptr<ConventionAst> &&conv) const -> std::shared_ptr<TypeAst> = 0;

    virtual auto without_generics() const -> std::shared_ptr<TypeAst> = 0;

    virtual auto substitute_generics(std::vector<GenericArgumentAst*> const &args) const -> std::shared_ptr<TypeAst> = 0;

    virtual auto contains_generic(GenericParameterAst const &generic) const -> bool = 0;

    virtual auto match_generic(TypeAst const &other, TypeIdentifierAst const &generic_name) const -> const ExpressionAst* = 0;

    virtual auto with_generics(std::shared_ptr<GenericArgumentGroupAst> &&arg_group) const -> std::shared_ptr<TypeAst> = 0;
};
