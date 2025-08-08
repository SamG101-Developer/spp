#ifndef ABSTRACT_TYPE_AST_HPP
#define ABSTRACT_TYPE_AST_HPP

#include <spp/asts/_fwd.hpp>

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
    virtual auto is_never_type() const -> bool = 0;

    virtual auto ns_parts() const -> std::vector<IdentifierAst const*> = 0;

    virtual auto type_parts() const -> std::vector<TypeIdentifierAst const*> = 0;

    virtual auto without_convention() const -> TypeAst const* = 0;

    virtual auto get_convention() const -> ConventionAst* = 0;

    virtual auto with_convention(std::unique_ptr<ConventionAst> &&convention) const -> std::unique_ptr<TypeAst> = 0;

    virtual auto without_generics() const -> std::unique_ptr<TypeAst> = 0;

    virtual auto substitute_generics(std::vector<GenericArgumentAst*> args) const -> std::unique_ptr<TypeAst> = 0;

    virtual auto contains_generic(TypeAst const *generic) const -> bool = 0;

    virtual auto match_generic(TypeAst const *other, TypeAst const *generic) -> TypeAst* = 0;

    virtual auto with_generics(std::unique_ptr<GenericArgumentGroupAst> &&arg_group) -> std::unique_ptr<TypeAst> = 0;
};


#endif //ABSTRACT_TYPE_AST_HPP
