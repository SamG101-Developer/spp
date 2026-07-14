module;
#include <spp/macros.hpp>

export module spp.asts.mixins.abstract_type_ast;
import spp.utils.types;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
}

namespace spp::asts::mixins {
    SPP_EXP_CLS struct AbstractTypeAst;
}

SPP_EXP_CLS struct spp::asts::mixins::AbstractTypeAst {
    AbstractTypeAst();

    virtual ~AbstractTypeAst();

    // SPP_ATTR_NODISCARD virtual auto Iterator() const -> Vec<TypeIdentifierAst*> = 0;

    SPP_ATTR_NODISCARD virtual auto IsNeverType() const noexcept -> bool = 0;

    SPP_ATTR_NODISCARD virtual auto NsParts() const -> Vec<IdentifierAst*> = 0;

    SPP_ATTR_NODISCARD virtual auto TypeParts() const -> Vec<TypeIdentifierAst*> = 0;

    SPP_ATTR_NODISCARD virtual auto WithoutConvention() const -> TypeAst* = 0;

    SPP_ATTR_NODISCARD virtual auto GetConvention() const -> ConventionAst* = 0;

    SPP_ATTR_NODISCARD virtual auto WithConvention(Unique<ConventionAst> &&conv) const -> Unique<TypeAst> = 0;

    SPP_ATTR_NODISCARD virtual auto WithoutGenerics() const -> Unique<TypeAst> = 0;

    SPP_ATTR_NODISCARD virtual auto SubstituteGenerics(Vec<GenericArgumentAst*> const &args) const -> Unique<TypeAst> = 0;

    SPP_ATTR_NODISCARD virtual auto ContainsGenerics(GenericParameterAst const &generic) const -> bool = 0;

    SPP_ATTR_NODISCARD virtual auto WithGenerics(Unique<GenericArgumentGroupAst> &&arg_group) const -> Unique<TypeAst> = 0;

    SPP_ATTR_NODISCARD virtual auto IsCompilerGeneratedType() const -> bool = 0;

    virtual auto ResetCache() -> void = 0;
};
