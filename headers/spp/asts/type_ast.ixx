module;
#include <spp/macros.hpp>

export module spp.asts:type_ast;
import :primary_expression_ast;
import std;

namespace spp::asts {
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypePostfixExpressionAst;
    SPP_EXP_CLS struct TypeUnaryExpressionAst;
}


/**
 * The TypeAst is a base class for all type-related AST nodes in the SPP language.
 */
SPP_EXP_CLS struct spp::asts::TypeAst : PrimaryExpressionAst, std::enable_shared_from_this<TypeAst> {
    TypeAst();

    ~TypeAst() override;


    SPP_ATTR_NODISCARD virtual auto iterator() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> = 0;

    SPP_ATTR_NODISCARD virtual auto is_never_type() const -> bool = 0;

    SPP_ATTR_NODISCARD virtual auto ns_parts() const -> std::vector<std::shared_ptr<const IdentifierAst>> = 0;

    SPP_ATTR_NODISCARD virtual auto ns_parts() -> std::vector<std::shared_ptr<IdentifierAst>> = 0;

    SPP_ATTR_NODISCARD virtual auto type_parts() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> = 0;

    SPP_ATTR_NODISCARD virtual auto type_parts() -> std::vector<std::shared_ptr<TypeIdentifierAst>> = 0;

    SPP_ATTR_NODISCARD virtual auto without_convention() const -> std::shared_ptr<const TypeAst> = 0;

    SPP_ATTR_NODISCARD virtual auto get_convention() const -> ConventionAst* = 0;

    SPP_ATTR_NODISCARD virtual auto with_convention(std::unique_ptr<ConventionAst> &&conv) const -> std::shared_ptr<TypeAst> = 0;

    SPP_ATTR_NODISCARD virtual auto without_generics() const -> std::shared_ptr<TypeAst> = 0;

    SPP_ATTR_NODISCARD virtual auto substitute_generics(std::vector<GenericArgumentAst*> const &args) const -> std::shared_ptr<TypeAst> = 0;

    SPP_ATTR_NODISCARD virtual auto contains_generic(GenericParameterAst const &generic) const -> bool = 0;

    SPP_ATTR_NODISCARD virtual auto with_generics(std::unique_ptr<GenericArgumentGroupAst> &&arg_group) const -> std::shared_ptr<TypeAst> = 0;
};
