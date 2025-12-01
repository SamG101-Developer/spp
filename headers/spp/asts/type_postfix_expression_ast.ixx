module;
#include <spp/macros.hpp>

export module spp.asts.type_postfix_expression_ast;
import spp.asts.type_postfix_expression_operator_ast;
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
    SPP_EXP_CLS struct TypePostfixExpressionAst;
}


SPP_EXP_CLS struct spp::asts::TypePostfixExpressionAst final : TypeAst {
    /**
     * The left-hand side type of the postfix expression. This is the base type on which the postfix operation is
     * applied.
     */
    std::shared_ptr<TypeAst> lhs;

    /**
     * The operator token that represents the postfix operation. This indicates the type of operation being performed.
     */
    std::unique_ptr<TypePostfixExpressionOperatorAst> tok_op;

    /**
     * Construct the PostfixExpressionAst with the arguments matching the members.
     * @param[in] lhs The left-hand side type of the postfix expression.
     * @param[in] tok_op The operator token that represents the postfix operation.
     */
    TypePostfixExpressionAst(
        decltype(lhs) &&lhs,
        decltype(tok_op) &&tok_op);

    ~TypePostfixExpressionAst() override;

protected:
    auto equals_type_postfix_expression(TypePostfixExpressionAst const &) const -> std::strong_ordering override;
    auto equals(const ExpressionAst &) const -> std::strong_ordering override;

public:
    SPP_AST_KEY_FUNCTIONS;

    SPP_ATTR_ALWAYS_INLINE auto operator<=>(TypePostfixExpressionAst const &other) const -> std::strong_ordering {
        return equals(other);
    }

    SPP_ATTR_ALWAYS_INLINE auto operator==(TypePostfixExpressionAst const &other) const -> bool {
        return equals(other) == std::strong_ordering::equal;
    }

    auto iterator() const -> std::vector<std::shared_ptr<const TypeIdentifierAst>> override;

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


spp::asts::TypePostfixExpressionAst::~TypePostfixExpressionAst() = default;
