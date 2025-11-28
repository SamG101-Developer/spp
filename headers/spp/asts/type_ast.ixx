module;
#include <genex/generator.hpp>

#include <spp/macros.hpp>

export module spp.asts.type_ast;
import spp.asts.primary_expression_ast;
import spp.asts.mixins.abstract_type_ast;

import std;

namespace spp::asts {
    SPP_EXP_CLS struct ConventionAst;
    SPP_EXP_CLS struct GenericArgumentAst;
    SPP_EXP_CLS struct GenericArgumentGroupAst;
    SPP_EXP_CLS struct GenericParameterAst;
    SPP_EXP_CLS struct IdentifierAst;
    SPP_EXP_CLS struct TypeAst;
    SPP_EXP_CLS struct TypeIdentifierAst;
    SPP_EXP_CLS struct TypePostfixExpressionAst;
    SPP_EXP_CLS struct TypeUnaryExpressionAst;
}


/**
 * The TypeAst is a base class for all type-related AST nodes in the SPP language.
 */
SPP_EXP_CLS struct spp::asts::TypeAst : PrimaryExpressionAst, mixins::AbstractTypeAst, std::enable_shared_from_this<TypeAst> {
    using PrimaryExpressionAst::PrimaryExpressionAst;

    ~TypeAst() override;

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

protected:
    SPP_ATTR_NODISCARD auto equals_type_identifier(TypeIdentifierAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals_type_unary_expression(TypeUnaryExpressionAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals_type_postfix_expression(TypePostfixExpressionAst const &) const -> std::strong_ordering override;
    SPP_ATTR_NODISCARD auto equals(ExpressionAst const &other) const -> std::strong_ordering override;
};
