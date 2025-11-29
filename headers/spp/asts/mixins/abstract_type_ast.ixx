module;
#include <spp/macros.hpp>

export module spp.asts.mixins.abstract_type_ast;
import genex;
import std;


namespace spp::asts::mixins {
    SPP_EXP_CLS struct AbstractTypeAst;
}

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}


SPP_EXP_CLS struct spp::asts::mixins::AbstractTypeAst {
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

    virtual auto with_generics(std::unique_ptr<GenericArgumentGroupAst> &&arg_group) const -> std::shared_ptr<TypeAst> = 0;
};
